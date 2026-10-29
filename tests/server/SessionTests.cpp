// End-to-end session tests: the real server systems (installSystems), its
// network thread, and real clients talking over the loopback interface. Server time is driven by
// GameLoop::runFrame, so a 5 second timeout takes no real time.

#include "ecs/Loop/GameLoop.hpp"
#include "ecs/System/System.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "game/Ship.hpp"
#include "network/Protocol/PacketCodec.hpp"
#include "network/Reliability/ReliableChannel.hpp"
#include "network/Transport/NetworkThread.hpp"
#include "network/Transport/UdpSocket.hpp"
#include "server/Components/ClientMessage.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/Player.hpp"
#include "server/Network.hpp"
#include "server/ServerSystems.hpp"
#include "server/Systems/NetReceiveSystem.hpp"
#include "server/Systems/SessionReceiveSystem.hpp"

#include <asio/ip/address_v4.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

using network::Endpoint;
using network::ReliableChannel;
using network::UdpSocket;
using namespace network::protocol;
using namespace std::chrono_literals;

namespace {

UdpSocket openOrFail()
{
    auto socket = UdpSocket::open(0);
    if (!socket) {
        ADD_FAILURE() << "cannot open socket: " << socket.error().message();
        std::abort();
    }
    return std::move(*socket);
}

/// The server's network thread, as r-type_server opens it.
std::unique_ptr<network::NetworkThread> openNetworkOrFail()
{
    auto network = network::NetworkThread::open(0);
    if (!network) {
        ADD_FAILURE() << "cannot open network thread: " << network.error().message();
        std::abort();
    }
    return std::move(*network);
}

/// Copies the ClientMessages seen this frame, before they are discarded.
class MessageRecorder final : public ecs::System {
public:
    explicit MessageRecorder(std::vector<Payload>& messages)
        : _messages(messages)
    {
    }

    void update(ecs::World& world, const ecs::Time& /*time*/) override
    {
        world.query<server::ClientMessage>(
            [&](ecs::Entity /*entity*/, server::ClientMessage& message) {
                _messages.push_back(message.payload);
            });
    }

private:
    std::vector<Payload>& _messages;
};

class TestServer {
public:
    TestServer()
    {
        server::installSystems(_world, _loop, *_network);
        _loop.frameStart().add<MessageRecorder>(messages); // After every server system
    }

    /// One server frame that lasted `seconds` of (simulated) time.
    void frame(float seconds = 0.001F)
    {
        _loop.runFrame(seconds);
    }

    [[nodiscard]]
    Endpoint address() const
    {
        return {asio::ip::address_v4::loopback(), _network->localEndpoint().port()};
    }

    [[nodiscard]]
    std::size_t sessionCount()
    {
        std::size_t count = 0;
        _world.query<server::NetClient>(
            [&](ecs::Entity /*entity*/, server::NetClient& /*client*/) { ++count; });
        return count;
    }

    /// The PlayerInput of player `playerId`, or nullptr if none yet.
    [[nodiscard]]
    const server::PlayerInput* playerInput(std::uint8_t playerId)
    {
        const server::PlayerInput* found = nullptr;
        _world.query<server::NetClient, server::PlayerInput>(
            [&](ecs::Entity /*entity*/, server::NetClient& client, server::PlayerInput& input) {
                if (client.playerId == playerId) {
                    found = &input;
                }
            });
        return found;
    }

    /// Position of the ship of player `playerId`, or nullopt if it has none.
    [[nodiscard]]
    std::optional<game::Position> shipPosition(std::uint8_t playerId)
    {
        std::optional<game::Position> found;
        _world.query<server::PlayerShip, game::Position>(
            [&](ecs::Entity /*entity*/, server::PlayerShip& ship, game::Position& position) {
                if (ship.playerId == playerId) {
                    found = position;
                }
            });
        return found;
    }

    [[nodiscard]]
    std::size_t shipCount()
    {
        std::size_t count = 0;
        _world.query<server::PlayerShip>(
            [&](ecs::Entity /*entity*/, server::PlayerShip& /*ship*/) { ++count; });
        return count;
    }

    template <typename System>
    [[nodiscard]]
    System& system()
    {
        return *_loop.frameStart().find<System>();
    }

    void shutdown()
    {
        server::notifyShutdown(_world, *_network, server::TimePoint{});
    }

    std::vector<Payload> messages; // ClientMessages, as gameplay would see them

private:
    // Declared first, so destroyed last: the thread outlives the systems using it
    std::unique_ptr<network::NetworkThread> _network = openNetworkOrFail();
    ecs::World _world;
    ecs::GameLoop _loop{_world};
};

/// A minimal client: a socket and a ReliableChannel, like the real one will be.
class TestClient {
public:
    explicit TestClient(Endpoint server)
        : _server(std::move(server))
    {
    }

    void send(Payload payload)
    {
        sendRaw(_channel.wrap(std::move(payload), ReliableChannel::Clock::now()));
    }

    void sendRaw(const Packet& packet)
    {
        DatagramBuffer buffer{};
        const auto size = encode(packet, buffer);
        ASSERT_TRUE(size.has_value());
        sendBytes(std::span(buffer).first(*size));
    }

    void sendBytes(std::span<const std::byte> bytes)
    {
        ASSERT_TRUE(_socket.sendTo(bytes, _server).has_value());
    }

    void connect(const std::string& name)
    {
        send(ConnectRequest{.name = name});
    }

    /// Reads every datagram waiting, in delivery order.
    void poll()
    {
        DatagramBuffer buffer{};
        while (true) {
            const auto received = _socket.receive(buffer);
            if (!received || !*received) {
                return;
            }
            const auto packet
                = decode(std::span(buffer).first((*received)->size), Direction::ServerToClient);
            ASSERT_TRUE(packet.has_value()) << "the server sent an invalid packet";
            if (std::holds_alternative<ConnectAccept>(packet->payload)) {
                token = packet->sessionToken;
                _channel.setSessionToken(token);
            }
            const auto payloads = _channel.receive(*packet, ReliableChannel::Clock::now());
            ASSERT_TRUE(payloads.has_value());
            inbox.insert(inbox.end(), payloads->begin(), payloads->end());
        }
    }

    /// Removes and returns the first received payload of type T, if any.
    template <typename T>
    std::optional<T> take()
    {
        for (auto it = inbox.begin(); it != inbox.end(); ++it) {
            if (const T* found = std::get_if<T>(&*it)) {
                T result = *found;
                inbox.erase(it);
                return result;
            }
        }
        return std::nullopt;
    }

    std::uint32_t token{0};
    std::vector<Payload> inbox;

private:
    UdpSocket _socket = openOrFail();
    Endpoint _server;
    ReliableChannel _channel;
};

/// Runs short server frames until `client` received a T (for up to a second).
template <typename T>
std::optional<T> waitFor(TestServer& server, TestClient& client)
{
    for (int i = 0; i < 1000; ++i) {
        server.frame();
        client.poll();
        if (auto found = client.take<T>()) {
            return found;
        }
        std::this_thread::sleep_for(1ms);
    }
    return std::nullopt;
}

/// Lets datagrams already sent reach the server, then runs one frame.
void deliver(TestServer& server, float seconds = 0.001F)
{
    std::this_thread::sleep_for(20ms);
    server.frame(seconds);
}

ConnectAccept connectOrFail(TestServer& server, TestClient& client, const std::string& name)
{
    client.connect(name);
    const auto accepted = waitFor<ConnectAccept>(server, client);
    EXPECT_TRUE(accepted.has_value()) << name << " was not accepted";
    return accepted.value_or(ConnectAccept{});
}

} // namespace

// --- Connecting (§8.1) ----------------------------------------------------------

TEST(Session, ConnectRequestIsAccepted)
{
    TestServer server;
    TestClient client(server.address());

    const ConnectAccept accepted = connectOrFail(server, client, "Thibani");

    EXPECT_EQ(accepted.playerId, 0);
    EXPECT_EQ(accepted.tickRate, 60);
    EXPECT_EQ(accepted.snapshotRate, 30);
    EXPECT_EQ(accepted.maxPlayers, 4);
    EXPECT_NE(client.token, 0U);
    EXPECT_EQ(server.sessionCount(), 1U);
}

TEST(Session, PlayersGetDistinctIdsAndTokens)
{
    TestServer server;
    TestClient first(server.address());
    TestClient second(server.address());

    EXPECT_EQ(connectOrFail(server, first, "One").playerId, 0);
    EXPECT_EQ(connectOrFail(server, second, "Two").playerId, 1);
    EXPECT_NE(first.token, second.token);
}

TEST(Session, EveryoneLearnsAboutEveryone)
{
    TestServer server;
    TestClient first(server.address());
    TestClient second(server.address());
    (void)connectOrFail(server, first, "One");
    (void)connectOrFail(server, second, "Two");

    const auto newcomer = waitFor<PlayerJoined>(server, first);
    const auto alreadyHere = waitFor<PlayerJoined>(server, second);

    ASSERT_TRUE(newcomer.has_value());
    EXPECT_EQ(*newcomer, (PlayerJoined{.playerId = 1, .name = "Two"}));
    ASSERT_TRUE(alreadyHere.has_value());
    EXPECT_EQ(*alreadyHere, (PlayerJoined{.playerId = 0, .name = "One"}));
}

TEST(Session, FifthPlayerIsRejected)
{
    TestServer server;
    std::vector<TestClient> players;
    players.reserve(4);
    for (int i = 0; i < 4; ++i) {
        players.emplace_back(server.address());
        (void)connectOrFail(server, players.back(), "Player" + std::to_string(i));
    }
    TestClient fifth(server.address());

    fifth.connect("Late");
    const auto rejected = waitFor<ConnectReject>(server, fifth);

    ASSERT_TRUE(rejected.has_value());
    EXPECT_EQ(rejected->reason, RejectReason::ServerFull);
    EXPECT_EQ(server.sessionCount(), 4U);
}

TEST(Session, EmptyNameIsRejected)
{
    TestServer server;
    TestClient client(server.address());

    client.connect("");
    const auto rejected = waitFor<ConnectReject>(server, client);

    ASSERT_TRUE(rejected.has_value());
    EXPECT_EQ(rejected->reason, RejectReason::InvalidName);
    EXPECT_EQ(server.sessionCount(), 0U);
}

TEST(Session, OtherProtocolVersionIsRejected)
{
    TestServer server;
    TestClient client(server.address());
    DatagramBuffer buffer{};
    const Packet request{
        .sessionToken = 0,
        .sequence = 0,
        .ack = std::nullopt,
        .reliableId = 0,
        .payload = ConnectRequest{.name = "Future"},
    };
    const auto size = encode(request, buffer);
    ASSERT_TRUE(size.has_value());
    buffer[2] = std::byte{2}; // version byte: a version 2 client

    client.sendBytes(std::span(buffer).first(*size));
    const auto rejected = waitFor<ConnectReject>(server, client);

    ASSERT_TRUE(rejected.has_value());
    EXPECT_EQ(rejected->reason, RejectReason::VersionMismatch);
}

TEST(Session, RepeatedRequestGetsTheSameAccept)
{
    // The first CONNECT_ACCEPT may be lost: the client asks again (§8.1)
    TestServer server;
    TestClient client(server.address());
    const ConnectAccept first = connectOrFail(server, client, "Thibani");
    const std::uint32_t firstToken = client.token;

    // Still token 0: a client retries because it never saw the ACCEPT
    client.sendRaw(Packet{
        .sessionToken = 0,
        .sequence = 1,
        .ack = std::nullopt,
        .reliableId = 0,
        .payload = ConnectRequest{.name = "Thibani"},
    });
    const auto second = waitFor<ConnectAccept>(server, client);
    ASSERT_TRUE(second.has_value());

    EXPECT_EQ(*second, first);
    EXPECT_EQ(client.token, firstToken);
    EXPECT_EQ(server.sessionCount(), 1U);
}

// --- Inside a session -----------------------------------------------------------

TEST(Session, InputReachesThePlayer)
{
    TestServer server;
    TestClient client(server.address());
    (void)connectOrFail(server, client, "Thibani");

    client.send(Input{.firstTick = 10, .count = 1, .masks = {input::Right}});
    deliver(server);

    const server::PlayerInput* player = server.playerInput(0);
    ASSERT_NE(player, nullptr);
    EXPECT_EQ(player->mask, input::Right);
    EXPECT_EQ(player->lastTick, 10U);
    EXPECT_TRUE(server.messages.empty()); // Consumed by InputSystem, not left over
}

TEST(Session, ConnectedPlayerGetsAShip)
{
    TestServer server;
    TestClient client(server.address());

    (void)connectOrFail(server, client, "Thibani");
    server.frame();

    EXPECT_EQ(server.shipCount(), 1U);
}

TEST(Session, ShipMovesWhereThePlayerSteers)
{
    // The whole chain: keys over UDP → PlayerInput → velocity → position
    TestServer server;
    TestClient client(server.address());
    (void)connectOrFail(server, client, "Thibani");
    server.frame();
    const auto start = server.shipPosition(0);
    ASSERT_TRUE(start.has_value());

    client.send(Input{.firstTick = 1, .count = 1, .masks = {input::Right}});
    deliver(server, 1.0F / 60.0F); // The input arrives, then one simulation step
    for (int step = 1; step < 60; ++step) {
        server.frame(1.0F / 60.0F); // The key stays held until the next INPUT
    }

    const auto end = server.shipPosition(0);
    ASSERT_TRUE(end.has_value());
    EXPECT_NEAR(end->x, start->x + game::ship::Speed, 1.0F);
    EXPECT_FLOAT_EQ(end->y, start->y);
}

TEST(Session, ShipGoesAwayWithItsPlayer)
{
    TestServer server;
    TestClient client(server.address());
    (void)connectOrFail(server, client, "Thibani");
    server.frame();

    client.send(Disconnect{.reason = DisconnectReason::Quit});
    deliver(server); // Session closed at the end of this frame
    server.frame(); // Its ship goes at the start of the next one

    EXPECT_EQ(server.shipCount(), 0U);
}

TEST(Session, PacketWithAWrongTokenIsIgnored)
{
    TestServer server;
    TestClient client(server.address());
    (void)connectOrFail(server, client, "Thibani");

    client.sendRaw(Packet{
        .sessionToken = client.token + 1,
        .sequence = 100,
        .ack = std::nullopt,
        .reliableId = 0,
        .payload = Input{.firstTick = 1, .count = 1, .masks = {}},
    });
    deliver(server);

    EXPECT_TRUE(server.messages.empty());
    EXPECT_EQ(server.system<server::SessionReceiveSystem>().unknownSenderCount(), 1U);
}

TEST(Session, ServerSendsHeartbeatsWhenIdle)
{
    TestServer server;
    TestClient client(server.address());
    (void)connectOrFail(server, client, "Thibani");
    std::this_thread::sleep_for(20ms);
    client.poll();
    client.send(Heartbeat{}); // Acknowledges the ENTITY_SPAWN: nothing left to resend
    deliver(server);

    server.frame(0.3F); // More than 250 ms without sending anything

    EXPECT_TRUE(waitFor<Heartbeat>(server, client).has_value());
}

// --- Four players, one shared game (§4.3, §5.3) --------------------------------

namespace {

/// Four connected players, each having received one ENTITY_SPAWN per ship.
struct FourPlayers {
    explicit FourPlayers(TestServer& server)
    {
        players.reserve(4);
        for (int i = 0; i < 4; ++i) {
            players.emplace_back(server.address());
            (void)connectOrFail(server, players.back(), "Player" + std::to_string(i));
        }
        spawns.resize(4);
        for (int frame = 0; frame < 1000 && !allSeeFourShips(); ++frame) {
            server.frame();
            std::this_thread::sleep_for(1ms);
            for (std::size_t i = 0; i < 4; ++i) {
                players[i].poll();
                while (auto spawn = players[i].take<EntitySpawn>()) {
                    spawns[i].push_back(*spawn);
                }
            }
        }
    }

    [[nodiscard]]
    bool allSeeFourShips() const
    {
        return std::ranges::all_of(spawns, [](const auto& seen) { return seen.size() == 4; });
    }

    std::vector<TestClient> players;
    std::vector<std::vector<EntitySpawn>> spawns; // Per player
};

} // namespace

TEST(Session, FourPlayersSeeTheSameFourShips)
{
    TestServer server;
    const FourPlayers game(server);

    ASSERT_TRUE(game.allSeeFourShips());
    for (const auto& seen : game.spawns) {
        std::set<std::uint8_t> owners;
        std::set<std::uint16_t> netIds;
        for (const EntitySpawn& spawn : seen) {
            EXPECT_EQ(spawn.kind, EntityKind::PlayerShip);
            owners.insert(spawn.owner);
            netIds.insert(spawn.netId);
        }
        EXPECT_EQ(owners, (std::set<std::uint8_t>{0, 1, 2, 3})); // One ship per colour
        EXPECT_EQ(netIds.size(), 4U);
        EXPECT_EQ(seen.size(), game.spawns[0].size());
    }
}

TEST(Session, SnapshotsCarryEveryShip)
{
    TestServer server;
    FourPlayers game(server);
    ASSERT_TRUE(game.allSeeFourShips());

    for (TestClient& player : game.players) {
        player.inbox.clear(); // Older snapshots may predate the last ship
    }
    server.frame(2.0F / 60.0F); // Two simulation steps: a snapshot is due
    for (TestClient& player : game.players) {
        const auto snapshot = waitFor<Snapshot>(server, player);
        ASSERT_TRUE(snapshot.has_value());
        EXPECT_EQ(snapshot->partCount, 1);
        std::set<std::uint8_t> owners;
        for (const EntityState& state : snapshot->entities) {
            if (state.kind != EntityKind::PlayerShip) {
                continue; // Bydos may already be there
            }
            owners.insert(state.owner);
            EXPECT_EQ(state.health, game::ship::MaxHealth); // 3 hearts each
        }
        EXPECT_EQ(owners, (std::set<std::uint8_t>{0, 1, 2, 3}));
    }
}

TEST(Session, SnapshotEchoesEachPlayersOwnInput)
{
    TestServer server;
    TestClient first(server.address());
    TestClient second(server.address());
    (void)connectOrFail(server, first, "One");
    (void)connectOrFail(server, second, "Two");

    first.send(Input{.firstTick = 5, .count = 1, .masks = {}});
    second.send(Input{.firstTick = 9, .count = 1, .masks = {}});
    deliver(server);
    server.frame(2.0F / 60.0F);
    first.poll();
    second.poll();
    first.inbox.clear(); // Only snapshots sent from now on: the inputs are in
    second.inbox.clear();

    const auto firstSnapshot = waitFor<Snapshot>(server, first);
    const auto secondSnapshot = waitFor<Snapshot>(server, second);
    ASSERT_TRUE(firstSnapshot.has_value());
    ASSERT_TRUE(secondSnapshot.has_value());
    EXPECT_EQ(firstSnapshot->lastInputTick, 5U);
    EXPECT_EQ(secondSnapshot->lastInputTick, 9U);
}

TEST(Session, LeavingShipIsDestroyedForTheOthers)
{
    TestServer server;
    FourPlayers game(server);
    ASSERT_TRUE(game.allSeeFourShips());
    const auto leavingShip
        = std::ranges::find(game.spawns[0], std::uint8_t{3}, &EntitySpawn::owner);
    ASSERT_NE(leavingShip, game.spawns[0].end());

    game.players[3].send(Disconnect{.reason = DisconnectReason::Quit});
    for (std::size_t i = 0; i < 3; ++i) {
        const auto destroyed = waitFor<EntityDestroy>(server, game.players[i]);
        ASSERT_TRUE(destroyed.has_value());
        EXPECT_EQ(destroyed->netId, leavingShip->netId);
    }
}

// --- A first game (#61) ----------------------------------------------------------

TEST(Session, FiringAndBydosReachTheClients)
{
    // The whole chain over UDP: FIRE → missile spawned with its shooter, and
    // Bydos entering the arena, as every client sees them
    TestServer server;
    TestClient client(server.address());
    (void)connectOrFail(server, client, "Thibani");

    std::optional<EntitySpawn> missile;
    std::optional<EntitySpawn> bydo;
    std::optional<GameState> state;
    for (int frame = 0; frame < 200 && !(missile && bydo && state); ++frame) {
        client.send(Input{.firstTick = static_cast<std::uint32_t>(frame + 1),
            .count = 1,
            .masks = {input::Fire},
        });
        std::this_thread::sleep_for(1ms);
        server.frame(1.0F / 60.0F);
        client.poll();
        while (auto spawn = client.take<EntitySpawn>()) {
            if (spawn->kind == EntityKind::PlayerMissile) {
                missile = spawn;
            } else if (spawn->kind == EntityKind::Bydo) {
                bydo = spawn;
            }
        }
        if (const auto received = client.take<GameState>()) {
            state = received;
        }
    }

    ASSERT_TRUE(missile.has_value()) << "no missile fired";
    EXPECT_EQ(missile->owner, 0);
    EXPECT_GT(missile->vx, 0);
    ASSERT_TRUE(bydo.has_value()) << "no Bydo arrived";
    EXPECT_EQ(bydo->owner, NoPlayer);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->phase, GamePhase::Running);
}

// --- Leaving (§8.2, §8.3) -------------------------------------------------------

TEST(Session, DisconnectIsAnnouncedToTheOthers)
{
    TestServer server;
    TestClient staying(server.address());
    TestClient leaving(server.address());
    (void)connectOrFail(server, staying, "Stays");
    (void)connectOrFail(server, leaving, "Leaves");

    leaving.send(Disconnect{.reason = DisconnectReason::Quit});
    const auto left = waitFor<PlayerLeft>(server, staying);

    ASSERT_TRUE(left.has_value());
    EXPECT_EQ(*left, (PlayerLeft{.playerId = 1, .reason = LeaveReason::Quit}));
    EXPECT_EQ(server.sessionCount(), 1U);
}

TEST(Session, FreedPlayerIdIsReused)
{
    TestServer server;
    TestClient first(server.address());
    TestClient second(server.address());
    (void)connectOrFail(server, first, "One");
    (void)connectOrFail(server, second, "Two");
    first.send(Disconnect{.reason = DisconnectReason::Quit});
    deliver(server);

    TestClient third(server.address());
    EXPECT_EQ(connectOrFail(server, third, "Three").playerId, 0);
}

TEST(Session, SilentClientTimesOutAndTheGameGoesOn)
{
    // The subject: a crashed client must not stop the server, and the others
    // must be told. "crashed" never sends anything after connecting.
    TestServer server;
    TestClient alive(server.address());
    TestClient crashed(server.address());
    (void)connectOrFail(server, alive, "Alive");
    (void)connectOrFail(server, crashed, "Crashed");

    // A live client reads every frame: its packets acknowledge PLAYER_JOINED.
    // Otherwise the server would rightly give up on it too after 5 s (§6.2).
    const auto keepAlive = [&](float seconds) {
        std::this_thread::sleep_for(20ms);
        alive.poll();
        alive.send(Heartbeat{});
        deliver(server, seconds);
    };

    keepAlive(3.0F); // 3 s later: both still connected
    EXPECT_EQ(server.sessionCount(), 2U);

    keepAlive(2.5F); // 5.5 s: "crashed" has been silent for too long

    EXPECT_EQ(server.sessionCount(), 1U);
    const auto left = waitFor<PlayerLeft>(server, alive);
    ASSERT_TRUE(left.has_value());
    EXPECT_EQ(*left, (PlayerLeft{.playerId = 1, .reason = LeaveReason::Timeout}));
}

TEST(Session, ShutdownIsAnnouncedToEveryClient)
{
    TestServer server;
    TestClient client(server.address());
    (void)connectOrFail(server, client, "Thibani");

    server.shutdown();
    std::this_thread::sleep_for(20ms);
    client.poll();

    const auto disconnect = client.take<Disconnect>();
    ASSERT_TRUE(disconnect.has_value());
    EXPECT_EQ(disconnect->reason, DisconnectReason::ServerShutdown);
}

// --- Protection (§7.1) ----------------------------------------------------------

TEST(Session, FloodingAddressIsRateLimited)
{
    TestServer server;
    TestClient flooder(server.address());

    for (int i = 0; i < 200; ++i) {
        flooder.send(Heartbeat{});
    }
    deliver(server);

    EXPECT_EQ(server.system<server::NetReceiveSystem>().rateLimitedCount(),
        200 - server::NetReceiveSystem::MaxPacketsPerSecond);
}
