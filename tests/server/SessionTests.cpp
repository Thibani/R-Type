// End-to-end session tests: the real server systems (installSystems), its
// network thread, and real clients talking over the loopback interface. Server time is driven by
// GameLoop::runFrame, so a 5 second timeout takes no real time.

#include "TemporaryFile.hpp"
#include "ecs/Loop/GameLoop.hpp"
#include "ecs/System/System.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "game/Ship.hpp"
#include "network/Crypto/SealedDatagram.hpp"
#include "network/Protocol/PacketCodec.hpp"
#include "network/Reliability/ReliableChannel.hpp"
#include "network/Transport/NetworkThread.hpp"
#include "network/Transport/UdpSocket.hpp"
#include "server/AccountStore.hpp"
#include "server/Components/ClientMessage.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/Player.hpp"
#include "server/FriendStore.hpp"
#include "server/MessageStore.hpp"
#include "server/Network.hpp"
#include "server/ServerSystems.hpp"
#include "server/Systems/ChatSystem.hpp"
#include "server/Systems/ConnectionSystem.hpp"
#include "server/Systems/LoginSystem.hpp"
#include "server/Systems/NetReceiveSystem.hpp"
#include "server/Systems/SessionReceiveSystem.hpp"

#include <asio/ip/address_v4.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
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

server::AccountStore loadOrFail(
    const std::filesystem::path& file, network::crypto::PasswordCost cost)
{
    auto accounts = server::AccountStore::load(file, cost);
    if (!accounts) {
        ADD_FAILURE() << "cannot load the accounts: " << accounts.error();
        std::abort();
    }
    return std::move(*accounts);
}

server::FriendStore loadFriendsOrFail(const std::filesystem::path& file)
{
    auto friends = server::FriendStore::load(file);
    if (!friends) {
        ADD_FAILURE() << "cannot load the friends: " << friends.error();
        std::abort();
    }
    return std::move(*friends);
}

server::MessageStore loadMessagesOrFail(const std::filesystem::path& file)
{
    auto messages = server::MessageStore::load(file);
    if (!messages) {
        ADD_FAILURE() << "cannot load the messages: " << messages.error();
        std::abort();
    }
    return std::move(*messages);
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
    /// By default with its own empty accounts file, and the cheapest hash.
    explicit TestServer(const std::filesystem::path* accountsFile = nullptr,
        network::crypto::PasswordCost cost = testing_players::FastCost)
        : _accounts(loadOrFail(accountsFile != nullptr ? *accountsFile : _ownFile.path(), cost))
        , _friends(loadFriendsOrFail(_friendsFile.path()))
        , _messages(loadMessagesOrFail(_messagesFile.path()))
    {
        server::installSystems(_world, _loop, *_network, _accounts, _friends, _messages);
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
        _world.query<server::InGame, server::PlayerInput>(
            [&](ecs::Entity /*entity*/, server::InGame& player, server::PlayerInput& input) {
                if (player.playerId == playerId) {
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

    [[nodiscard]]
    const std::filesystem::path& messagesFile() const noexcept
    {
        return _messagesFile.path();
    }

    [[nodiscard]]
    const std::filesystem::path& friendsFile() const noexcept
    {
        return _friendsFile.path();
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
    testing_players::TemporaryFile _ownFile;
    testing_players::TemporaryFile _friendsFile;
    testing_players::TemporaryFile _messagesFile;
    server::AccountStore _accounts;
    server::FriendStore _friends;
    server::MessageStore _messages;
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

    /// Sends a reliable payload (JOIN_GAME, LEAVE_GAME), and what is due with it.
    void sendReliable(Payload payload)
    {
        const auto now = ReliableChannel::Clock::now();
        _channel.queueReliable(std::move(payload));
        for (const Packet& packet : _channel.takeDue(now)) {
            sendRaw(packet);
        }
    }

    /// Sends `packet` as a real client would: sealed if it belongs to a session.
    void sendRaw(const Packet& packet)
    {
        const auto datagram = packet.sessionToken != 0 ? sealed(packet) : inClear(packet);
        sendBytes(datagram);
    }

    /// The datagram of `packet`, without encryption.
    [[nodiscard]]
    static std::vector<std::byte> inClear(const Packet& packet)
    {
        DatagramBuffer buffer{};
        const auto size = encode(packet, buffer);
        EXPECT_TRUE(size.has_value());
        return {buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(size.value_or(0))};
    }

    /// The datagram of `packet`, sealed with this client's key (connected only).
    [[nodiscard]]
    std::vector<std::byte> sealed(const Packet& packet)
    {
        const auto plain = inClear(packet);
        DatagramBuffer buffer{};
        const auto size = network::crypto::sealDatagram(*_cipher, plain, buffer);
        EXPECT_TRUE(size.has_value());
        return {buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(size.value_or(0))};
    }

    /// The next packet of this session, not sent yet.
    [[nodiscard]]
    Packet wrap(Payload payload)
    {
        return _channel.wrap(std::move(payload), ReliableChannel::Clock::now());
    }

    void sendBytes(std::span<const std::byte> bytes)
    {
        ASSERT_TRUE(_socket.sendTo(bytes, _server).has_value());
    }

    void connect(const std::string& name)
    {
        send(ConnectRequest{.name = name, .publicKey = keys.publicKey});
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
            std::span<const std::byte> datagram = std::span(buffer).first((*received)->size);
            rawDatagrams.emplace_back(datagram.begin(), datagram.end());

            // Only the connection answers travel in clear (§3.3)
            const auto header = decodeHeader(datagram);
            ASSERT_TRUE(header.has_value());
            const bool connectionAnswer
                = header->type == std::to_underlying(PacketType::ConnectAccept)
                || header->type == std::to_underlying(PacketType::ConnectReject);
            DatagramBuffer opened{};
            if (!connectionAnswer) {
                ASSERT_TRUE(_cipher.has_value()) << "sealed packet before CONNECT_ACCEPT";
                const auto size = network::crypto::openDatagram(*_cipher, datagram, opened);
                ASSERT_TRUE(size.has_value()) << "the server's packet cannot be opened";
                datagram = std::span(opened).first(*size);
            }

            const auto packet = decode(datagram, Direction::ServerToClient);
            ASSERT_TRUE(packet.has_value()) << "the server sent an invalid packet";
            if (const auto* accept = std::get_if<ConnectAccept>(&packet->payload)) {
                token = packet->sessionToken;
                _channel.setSessionToken(token);
                if (!_cipher) {
                    const auto key = network::crypto::deriveSessionKey(
                        keys, accept->publicKey, network::crypto::Role::Client);
                    ASSERT_TRUE(key.has_value());
                    _cipher.emplace(*key, network::crypto::Role::Client);
                }
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
    std::vector<std::vector<std::byte>> rawDatagrams; // As they came off the network
    network::crypto::KeyPair keys = network::crypto::generateKeyPair();

private:
    UdpSocket _socket = openOrFail();
    Endpoint _server;
    ReliableChannel _channel;
    std::optional<network::crypto::SessionCipher> _cipher;
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

constexpr std::string_view TestPassword = "correct horse";

/// Up to CONNECT_ACCEPT: an encrypted session, not part of the game yet.
ConnectAccept openSessionOrFail(TestServer& server, TestClient& client, const std::string& name)
{
    client.connect(name);
    const auto accepted = waitFor<ConnectAccept>(server, client);
    EXPECT_TRUE(accepted.has_value()) << name << " was not accepted";
    return accepted.value_or(ConnectAccept{});
}

/// What the server answers to one AUTH_REQUEST.
AuthCode authenticate(
    TestServer& server, TestClient& client, AuthMode mode, std::string_view password = TestPassword)
{
    client.send(AuthRequest{.mode = mode, .password = std::string(password)});
    const auto result = waitFor<AuthResult>(server, client);
    EXPECT_TRUE(result.has_value()) << "no AUTH_RESULT";
    return result.value_or(AuthResult{.code = AuthCode::ServerError}).code;
}

/// A new account, on the main page.
void logInOrFail(TestServer& server, TestClient& client, const std::string& name)
{
    (void)openSessionOrFail(server, client, name);
    EXPECT_EQ(authenticate(server, client, AuthMode::Register), AuthCode::Ok);
}

/// What the server answers to JOIN_GAME.
JoinResult joinOrFail(TestServer& server, TestClient& client)
{
    client.sendReliable(JoinGame{});
    const auto result = waitFor<JoinResult>(server, client);
    EXPECT_TRUE(result.has_value()) << "no JOIN_RESULT";
    return result.value_or(JoinResult{.code = JoinCode::GameFull, .playerId = NoPlayer});
}

/// A new account and a player in the game, as most tests need.
JoinResult connectOrFail(TestServer& server, TestClient& client, const std::string& name)
{
    logInOrFail(server, client, name);
    const JoinResult joined = joinOrFail(server, client);
    EXPECT_EQ(joined.code, JoinCode::Ok);
    return joined;
}

} // namespace

// --- Connecting (§8.1) ----------------------------------------------------------

TEST(Session, ConnectRequestIsAccepted)
{
    TestServer server;
    TestClient client(server.address());

    const ConnectAccept accepted = openSessionOrFail(server, client, "Thibani");

    EXPECT_EQ(accepted.playerId, NoPlayer); // A place in the game comes with JOIN_RESULT
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

TEST(Session, SessionsBeyondTheLimitAreRejected)
{
    TestServer server;
    std::vector<TestClient> clients;
    clients.reserve(server::ConnectionSystem::MaxSessions);
    for (std::size_t i = 0; i < server::ConnectionSystem::MaxSessions; ++i) {
        clients.emplace_back(server.address());
        (void)openSessionOrFail(server, clients.back(), "Client" + std::to_string(i));
    }
    TestClient late(server.address());

    late.connect("Late");
    const auto rejected = waitFor<ConnectReject>(server, late);

    ASSERT_TRUE(rejected.has_value());
    EXPECT_EQ(rejected->reason, RejectReason::ServerFull);
    EXPECT_EQ(server.sessionCount(), server::ConnectionSystem::MaxSessions);
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
    buffer[2] = static_cast<std::byte>(Version + 1); // version byte: a future client

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
    const ConnectAccept first = openSessionOrFail(server, client, "Thibani");
    const std::uint32_t firstToken = client.token;

    // Still token 0: a client retries because it never saw the ACCEPT
    client.sendRaw(Packet{
        .sessionToken = 0,
        .sequence = 1,
        .ack = std::nullopt,
        .reliableId = 0,
        .payload = ConnectRequest{.name = "Thibani", .publicKey = client.keys.publicKey},
    });
    const auto second = waitFor<ConnectAccept>(server, client);
    ASSERT_TRUE(second.has_value());

    EXPECT_EQ(*second, first);
    EXPECT_EQ(client.token, firstToken);
    EXPECT_EQ(server.sessionCount(), 1U);
}

// --- Encryption (§3.3) -----------------------------------------------------------

TEST(SessionEncryption, EachSessionGetsItsOwnServerKey)
{
    TestServer server;
    TestClient first(server.address());
    TestClient second(server.address());

    const ConnectAccept firstAccept = openSessionOrFail(server, first, "First");
    const ConnectAccept secondAccept = openSessionOrFail(server, second, "Second");

    EXPECT_NE(firstAccept.publicKey, secondAccept.publicKey);
}

TEST(SessionEncryption, InvalidClientKeyGetsNoSession)
{
    TestServer server;
    TestClient client(server.address());
    client.keys.publicKey = {}; // All zeros: the exchange would give a key anybody can compute

    client.connect("Mallory");
    deliver(server);
    client.poll();

    EXPECT_EQ(server.sessionCount(), 0U);
    EXPECT_TRUE(client.inbox.empty());
}

TEST(SessionEncryption, RequestWithAnotherKeyDoesNotTakeOverTheSession)
{
    TestServer server;
    TestClient client(server.address());
    (void)connectOrFail(server, client, "Thibani");
    client.inbox.clear();

    // Same address (anybody can fake it) but not the same key
    const network::crypto::KeyPair attacker = network::crypto::generateKeyPair();
    client.sendRaw(Packet{
        .sessionToken = 0,
        .sequence = 1,
        .ack = std::nullopt,
        .reliableId = 0,
        .payload = ConnectRequest{.name = "Thibani", .publicKey = attacker.publicKey},
    });
    deliver(server);
    client.poll();
    EXPECT_FALSE(client.take<ConnectAccept>().has_value()) << "the attacker must not get an answer";

    // The real client's key still works
    client.send(Input{.firstTick = 1, .count = 1, .masks = {input::Right}});
    deliver(server);
    const server::PlayerInput* player = server.playerInput(0);
    ASSERT_NE(player, nullptr);
    EXPECT_EQ(player->lastTick, 1U);
}

TEST(SessionEncryption, PacketInClearIsDroppedEvenWithTheRightToken)
{
    // The token travels in clear, so an attacker knows it
    TestServer server;
    TestClient client(server.address());
    (void)connectOrFail(server, client, "Thibani");

    client.sendBytes(TestClient::inClear(
        client.wrap(Input{.firstTick = 1, .count = 1, .masks = {input::Right}})));
    deliver(server);

    EXPECT_EQ(server.playerInput(0), nullptr) << "the input reached the player";
    EXPECT_EQ(server.system<server::NetReceiveSystem>().droppedCount(), 1U);
}

TEST(SessionEncryption, ModifiedPacketIsDropped)
{
    TestServer server;
    TestClient client(server.address());
    (void)connectOrFail(server, client, "Thibani");

    auto datagram
        = client.sealed(client.wrap(Input{.firstTick = 1, .count = 1, .masks = {input::Right}}));
    datagram.back() ^= std::byte{0x01};
    client.sendBytes(datagram);
    deliver(server);

    EXPECT_EQ(server.playerInput(0), nullptr) << "the input reached the player";
    EXPECT_EQ(server.system<server::NetReceiveSystem>().droppedCount(), 1U);
}

TEST(SessionEncryption, ReplayedPacketIsDropped)
{
    TestServer server;
    TestClient client(server.address());
    (void)connectOrFail(server, client, "Thibani");

    const auto datagram
        = client.sealed(client.wrap(Input{.firstTick = 1, .count = 1, .masks = {input::Right}}));
    client.sendBytes(datagram);
    client.sendBytes(datagram);
    deliver(server);

    const server::PlayerInput* player = server.playerInput(0);
    ASSERT_NE(player, nullptr);
    EXPECT_EQ(player->lastTick, 1U); // The first copy did arrive
    EXPECT_EQ(server.system<server::NetReceiveSystem>().droppedCount(), 1U);
}

TEST(SessionEncryption, ForgedPacketsDoNotKeepASessionAlive)
{
    TestServer server;
    TestClient client(server.address());
    (void)connectOrFail(server, client, "Thibani");

    client.sendBytes(TestClient::inClear(client.wrap(Heartbeat{})));
    deliver(server, 3.0F);
    client.sendBytes(TestClient::inClear(client.wrap(Heartbeat{})));
    deliver(server, 3.0F); // 6 seconds with only forged packets: more than the 5 s timeout
    server.frame();

    EXPECT_EQ(server.sessionCount(), 0U);
}

TEST(SessionEncryption, NothingOfTheGameIsReadableOnTheNetwork)
{
    TestServer server;
    TestClient watcher(server.address());
    (void)connectOrFail(server, watcher, "Watcher");
    TestClient newcomer(server.address());
    (void)connectOrFail(server, newcomer, "SecretName");

    (void)waitFor<PlayerJoined>(server, watcher); // Carries the name "SecretName"
    ASSERT_FALSE(watcher.rawDatagrams.empty());

    const std::string_view name = "SecretName";
    for (const auto& datagram : watcher.rawDatagrams) {
        const std::string_view text(
            reinterpret_cast<const char*>(datagram.data()), datagram.size());
        EXPECT_EQ(text.find(name), std::string_view::npos);
    }
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

    client.sendBytes(TestClient::inClear(Packet{
        .sessionToken = client.token + 1,
        .sequence = 100,
        .ack = std::nullopt,
        .reliableId = 0,
        .payload = Input{.firstTick = 1, .count = 1, .masks = {}},
    }));
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

// --- Accounts (§8.1, issue #73) -------------------------------------------------

TEST(Login, CreatingAnAccountOpensTheMainPage)
{
    TestServer server;
    TestClient client(server.address());

    (void)openSessionOrFail(server, client, "Dave");
    EXPECT_EQ(authenticate(server, client, AuthMode::Register), AuthCode::Ok);
    server.frame();

    EXPECT_EQ(server.sessionCount(), 1U);
    EXPECT_EQ(server.shipCount(), 0U);
}

TEST(Login, CreatedAccountCanLogInAgain)
{
    TestServer server;
    {
        TestClient first(server.address());
        (void)connectOrFail(server, first, "Dave");
        first.send(Disconnect{.reason = DisconnectReason::Quit});
        deliver(server);
    }
    TestClient second(server.address());

    (void)openSessionOrFail(server, second, "Dave");

    EXPECT_EQ(authenticate(server, second, AuthMode::Login), AuthCode::Ok);
}

TEST(Login, WrongPasswordIsRefused)
{
    TestServer server;
    TestClient owner(server.address());
    (void)connectOrFail(server, owner, "Dave");
    TestClient other(server.address());
    (void)openSessionOrFail(server, other, "Dave");

    EXPECT_EQ(authenticate(server, other, AuthMode::Login, "not the password"),
        AuthCode::WrongCredentials);
    server.frame();

    EXPECT_EQ(server.shipCount(), 1U);
}

TEST(Login, UnknownNameGetsTheSameAnswerAsAWrongPassword)
{
    TestServer server;
    TestClient client(server.address());
    (void)openSessionOrFail(server, client, "Nobody");

    EXPECT_EQ(authenticate(server, client, AuthMode::Login), AuthCode::WrongCredentials);
    server.frame();

    EXPECT_EQ(server.shipCount(), 0U);
}

TEST(Login, NameAlreadyTakenIsRefused)
{
    TestServer server;
    TestClient owner(server.address());
    (void)connectOrFail(server, owner, "Dave");
    TestClient other(server.address());
    (void)openSessionOrFail(server, other, "Dave");

    EXPECT_EQ(authenticate(server, other, AuthMode::Register), AuthCode::NameTaken);
}

TEST(Login, ShortPasswordIsRefusedAtCreation)
{
    TestServer server;
    TestClient client(server.address());
    (void)openSessionOrFail(server, client, "Dave");

    EXPECT_EQ(
        authenticate(server, client, AuthMode::Register, "1234567"), AuthCode::InvalidPassword);
}

TEST(Login, NameWithAControlCharacterIsRefusedAtCreation)
{
    TestServer server;
    TestClient client(server.address());
    (void)openSessionOrFail(server, client, "Da\nve");

    EXPECT_EQ(authenticate(server, client, AuthMode::Register), AuthCode::InvalidName);
}

TEST(Login, SecondConnectionToAnAccountIsRefused)
{
    TestServer server;
    TestClient first(server.address());
    (void)connectOrFail(server, first, "Dave");
    TestClient second(server.address());
    (void)openSessionOrFail(server, second, "Dave");

    EXPECT_EQ(authenticate(server, second, AuthMode::Login), AuthCode::AlreadyConnected);
    server.frame();

    EXPECT_EQ(server.shipCount(), 1U);
}

TEST(Login, OnlyOneAttemptPerConnection)
{
    TestServer server;
    TestClient owner(server.address());
    (void)connectOrFail(server, owner, "Dave");
    TestClient guesser(server.address());
    (void)openSessionOrFail(server, guesser, "Dave");
    ASSERT_EQ(
        authenticate(server, guesser, AuthMode::Login, "first guess"), AuthCode::WrongCredentials);

    guesser.send(AuthRequest{.mode = AuthMode::Login, .password = std::string(TestPassword)});

    EXPECT_FALSE(waitFor<AuthResult>(server, guesser).has_value());
}

TEST(Login, RepeatedRequestGetsOneAnswer)
{
    TestServer server;
    TestClient client(server.address());
    (void)openSessionOrFail(server, client, "Dave");
    const AuthRequest request{.mode = AuthMode::Register, .password = std::string(TestPassword)};

    client.send(request);
    client.send(request); // The first one seems lost: the client sends it again
    ASSERT_TRUE(waitFor<AuthResult>(server, client).has_value());

    EXPECT_FALSE(waitFor<AuthResult>(server, client).has_value());
}

TEST(Login, AccountsSurviveAServerRestart)
{
    const testing_players::TemporaryFile file;
    {
        TestServer server(&file.path());
        TestClient client(server.address());
        (void)connectOrFail(server, client, "Dave");
    }
    TestServer restarted(&file.path());
    TestClient client(restarted.address());
    (void)openSessionOrFail(restarted, client, "Dave");

    EXPECT_EQ(authenticate(restarted, client, AuthMode::Login), AuthCode::Ok);
}

TEST(Login, PasswordIsNeitherInTheFileNorOnTheNetwork)
{
    const testing_players::TemporaryFile file;
    TestServer server(&file.path());
    TestClient client(server.address());
    (void)connectOrFail(server, client, "Dave");

    std::ifstream input(file.path(), std::ios::binary);
    const std::string content{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    EXPECT_NE(content.find("Dave"), std::string::npos);
    EXPECT_EQ(content.find(TestPassword), std::string::npos);

    const auto datagram = client.sealed(
        client.wrap(AuthRequest{.mode = AuthMode::Login, .password = std::string(TestPassword)}));
    const auto* const bytes = reinterpret_cast<const char*>(datagram.data()); // NOLINT
    EXPECT_EQ(std::string_view(bytes, datagram.size()).find(TestPassword), std::string_view::npos);
}

TEST(Login, EveryPasswordGivesARequestOfTheSameSize)
{
    TestServer server;
    TestClient client(server.address());
    (void)openSessionOrFail(server, client, "Dave");

    const auto sizeWith = [&](const std::string& password) {
        return client
            .sealed(client.wrap(AuthRequest{.mode = AuthMode::Login, .password = password}))
            .size();
    };

    EXPECT_EQ(sizeWith("12345678"), sizeWith(std::string(64, 'x'))); // Its length stays hidden
}

TEST(Login, RequestInClearIsDropped)
{
    TestServer server;
    TestClient client(server.address());
    (void)openSessionOrFail(server, client, "Dave");

    client.sendBytes(TestClient::inClear(client.wrap(
        AuthRequest{.mode = AuthMode::Register, .password = std::string(TestPassword)})));

    EXPECT_FALSE(waitFor<AuthResult>(server, client).has_value());
    EXPECT_EQ(server.shipCount(), 0U);
}

TEST(Login, NothingOfTheGameBeforeLoggingIn)
{
    TestServer server;
    TestClient player(server.address());
    (void)connectOrFail(server, player, "Dave");
    TestClient stranger(server.address());
    (void)openSessionOrFail(server, stranger, "Stranger");

    stranger.send(Input{.firstTick = 1, .count = 1, .masks = {input::Fire}});
    for (int i = 0; i < 50; ++i) {
        server.frame(0.05F); // Enough time for snapshots and spawns
        std::this_thread::sleep_for(1ms);
        stranger.poll();
    }

    EXPECT_FALSE(stranger.take<Snapshot>().has_value());
    EXPECT_FALSE(stranger.take<EntitySpawn>().has_value());
    EXPECT_FALSE(stranger.take<PlayerJoined>().has_value());
    EXPECT_FALSE(stranger.take<GameState>().has_value());
    EXPECT_EQ(server.playerInput(1), nullptr);
    EXPECT_EQ(server.shipCount(), 1U);
    player.poll();
    while (const auto joined = player.take<PlayerJoined>()) {
        EXPECT_NE(joined->name, "Stranger");
    }
}

TEST(Login, AuthResultComesBeforeEverythingElse)
{
    TestServer server;
    TestClient first(server.address());
    (void)connectOrFail(server, first, "One");
    TestClient second(server.address());
    (void)openSessionOrFail(server, second, "Two");

    second.send(AuthRequest{.mode = AuthMode::Register, .password = std::string(TestPassword)});
    const auto isEvent = [](const Payload& payload) {
        return !std::holds_alternative<Heartbeat>(payload)
            && !std::holds_alternative<Snapshot>(payload)
            && !std::holds_alternative<ConnectAccept>(payload);
    };
    for (int i = 0; i < 1000 && !std::ranges::any_of(second.inbox, isEvent); ++i) {
        server.frame();
        second.poll();
        std::this_thread::sleep_for(1ms);
    }

    const auto firstEvent = std::ranges::find_if(second.inbox, isEvent);
    ASSERT_NE(firstEvent, second.inbox.end());
    EXPECT_TRUE(std::holds_alternative<AuthResult>(*firstEvent));
}

TEST(Login, LeavingBeforeLoggingInIsNotAnnounced)
{
    TestServer server;
    TestClient player(server.address());
    (void)connectOrFail(server, player, "Dave");
    TestClient stranger(server.address());
    (void)openSessionOrFail(server, stranger, "Stranger");

    stranger.send(Disconnect{.reason = DisconnectReason::Quit});
    deliver(server);
    deliver(server);

    EXPECT_EQ(server.sessionCount(), 1U);
    EXPECT_FALSE(waitFor<PlayerLeft>(server, player).has_value());
}

TEST(Login, SessionWithoutLoginIsClosedAfterTheDelay)
{
    TestServer server;
    TestClient stranger(server.address());
    (void)openSessionOrFail(server, stranger, "Stranger");

    // The heartbeats keep it alive: only the login delay can close it
    const float seconds = std::chrono::duration<float>(server::LoginSystem::LoginTimeout).count();
    for (int second = 0; second < static_cast<int>(seconds); ++second) {
        EXPECT_EQ(server.sessionCount(), 1U) << second;
        stranger.send(Heartbeat{});
        std::this_thread::sleep_for(5ms);
        server.frame(1.0F);
    }
    server.frame();

    EXPECT_EQ(server.sessionCount(), 0U);
    EXPECT_TRUE(waitFor<Disconnect>(server, stranger).has_value());
}

TEST(Login, TheGameDoesNotFreezeDuringTheHash)
{
    const testing_players::TemporaryFile file;
    TestServer server(&file.path(), network::crypto::DefaultPasswordCost); // The real, slow hash
    TestClient client(server.address());
    (void)openSessionOrFail(server, client, "Dave");

    const auto start = std::chrono::steady_clock::now();
    client.send(AuthRequest{.mode = AuthMode::Register, .password = std::string(TestPassword)});
    std::chrono::steady_clock::duration longestFrame{};
    std::optional<AuthResult> result;
    while (!result && std::chrono::steady_clock::now() - start < 30s) {
        const auto before = std::chrono::steady_clock::now();
        server.frame();
        longestFrame = std::max(longestFrame, std::chrono::steady_clock::now() - before);
        client.poll();
        result = client.take<AuthResult>();
        std::this_thread::sleep_for(1ms); // Each frame counts 1 ms: time passes as in real life
    }
    const auto total = std::chrono::steady_clock::now() - start;

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->code, AuthCode::Ok);
    EXPECT_LT(longestFrame, total / 2); // Hashing on the game thread: one frame would take it all
}

// --- Main page and game (§8.4) ---------------------------------------------------

TEST(MainPage, MorePlayersThanTheGameCanBeConnected)
{
    TestServer server;
    std::vector<TestClient> clients;
    clients.reserve(6);
    for (int i = 0; i < 6; ++i) {
        clients.emplace_back(server.address());
        logInOrFail(server, clients.back(), "Player" + std::to_string(i));
    }

    EXPECT_EQ(server.sessionCount(), 6U);
    EXPECT_EQ(server.shipCount(), 0U);
}

TEST(MainPage, NothingOfTheGameReachesThePage)
{
    TestServer server;
    TestClient player(server.address());
    (void)connectOrFail(server, player, "Dave");
    TestClient waiting(server.address());
    logInOrFail(server, waiting, "Eve");

    waiting.send(Input{.firstTick = 1, .count = 1, .masks = {input::Fire}});
    for (int i = 0; i < 50; ++i) {
        server.frame(0.05F); // Enough time for snapshots and spawns
        std::this_thread::sleep_for(1ms);
        waiting.poll();
    }

    EXPECT_FALSE(waiting.take<Snapshot>().has_value());
    EXPECT_FALSE(waiting.take<EntitySpawn>().has_value());
    EXPECT_FALSE(waiting.take<PlayerJoined>().has_value());
    EXPECT_FALSE(waiting.take<GameState>().has_value());
    EXPECT_EQ(server.playerInput(1), nullptr);
    EXPECT_EQ(server.shipCount(), 1U);
}

TEST(MainPage, TheSessionStaysAliveOnThePage)
{
    TestServer server;
    TestClient waiting(server.address());
    logInOrFail(server, waiting, "Eve");

    for (int second = 0; second < 8; ++second) {
        waiting.send(Heartbeat{});
        deliver(server, 1.0F);
    }

    EXPECT_EQ(server.sessionCount(), 1U);
}

TEST(MainPage, LeavingThePageIsNotAnnounced)
{
    TestServer server;
    TestClient player(server.address());
    (void)connectOrFail(server, player, "Dave");
    TestClient waiting(server.address());
    logInOrFail(server, waiting, "Eve");

    waiting.send(Disconnect{.reason = DisconnectReason::Quit});
    deliver(server);

    EXPECT_EQ(server.sessionCount(), 1U);
    EXPECT_FALSE(waitFor<PlayerLeft>(server, player).has_value());
}

TEST(JoinGame, PlayersLearnAboutANewcomerOnlyOnceInTheGame)
{
    TestServer server;
    TestClient first(server.address());
    (void)connectOrFail(server, first, "One");
    TestClient second(server.address());
    logInOrFail(server, second, "Two");
    EXPECT_FALSE(waitFor<PlayerJoined>(server, first).has_value());

    EXPECT_EQ(joinOrFail(server, second).playerId, 1);

    const auto joined = waitFor<PlayerJoined>(server, first);
    ASSERT_TRUE(joined.has_value());
    EXPECT_EQ(joined->name, "Two");
}

TEST(JoinGame, ResultComesBeforeTheEventsOfTheGame)
{
    TestServer server;
    TestClient first(server.address());
    (void)connectOrFail(server, first, "One");
    TestClient second(server.address());
    logInOrFail(server, second, "Two");
    second.inbox.clear();

    second.sendReliable(JoinGame{});
    ASSERT_TRUE(waitFor<GameState>(server, second).has_value());

    const auto firstEvent = std::ranges::find_if(second.inbox,
        [](const Payload& payload) { return !std::holds_alternative<Heartbeat>(payload); });
    ASSERT_NE(firstEvent, second.inbox.end());
    EXPECT_TRUE(std::holds_alternative<JoinResult>(*firstEvent));
}

TEST(JoinGame, FifthPlayerGetsGameFullAndStaysConnected)
{
    TestServer server;
    FourPlayers game(server);
    TestClient fifth(server.address());
    logInOrFail(server, fifth, "Late");

    const JoinResult result = joinOrFail(server, fifth);

    EXPECT_EQ(result, (JoinResult{.code = JoinCode::GameFull, .playerId = NoPlayer}));
    EXPECT_EQ(server.sessionCount(), 5U);
    EXPECT_EQ(server.shipCount(), 4U);
}

TEST(JoinGame, PlaceFreedByALeaverCanBeTaken)
{
    TestServer server;
    FourPlayers game(server);
    TestClient fifth(server.address());
    logInOrFail(server, fifth, "Late");
    ASSERT_EQ(joinOrFail(server, fifth).code, JoinCode::GameFull);

    game.players[1].sendReliable(LeaveGame{});
    ASSERT_TRUE(waitFor<PlayerLeft>(server, game.players[0]).has_value());

    EXPECT_EQ(joinOrFail(server, fifth), (JoinResult{.code = JoinCode::Ok, .playerId = 1}));
}

TEST(JoinGame, PlaceFreedInTheSameFrameIsNotGivenAway)
{
    // Otherwise the newcomer would inherit the ship of the player who left
    TestServer server;
    FourPlayers game(server);
    TestClient fifth(server.address());
    logInOrFail(server, fifth, "Late");

    game.players[1].sendReliable(LeaveGame{});
    fifth.sendReliable(JoinGame{});
    std::this_thread::sleep_for(20ms);
    const auto result = waitFor<JoinResult>(server, fifth);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->code, JoinCode::GameFull);
    EXPECT_EQ(server.shipCount(), 3U);
}

TEST(JoinGame, AskingTwiceGivesOneShipAndOneAnswer)
{
    TestServer server;
    TestClient client(server.address());
    (void)connectOrFail(server, client, "Dave");

    client.sendReliable(JoinGame{});
    deliver(server);
    deliver(server);
    client.poll();

    EXPECT_FALSE(client.take<JoinResult>().has_value());
    EXPECT_EQ(server.shipCount(), 1U);
}

TEST(JoinGame, BeforeLoggingInItIsIgnored)
{
    TestServer server;
    TestClient stranger(server.address());
    (void)openSessionOrFail(server, stranger, "Stranger");

    stranger.sendReliable(JoinGame{});
    deliver(server);
    deliver(server);
    stranger.poll();

    EXPECT_FALSE(stranger.take<JoinResult>().has_value());
    EXPECT_EQ(server.shipCount(), 0U);
}

TEST(LeaveGame, ShipGoesAndTheOthersAreTold)
{
    TestServer server;
    TestClient staying(server.address());
    TestClient leaving(server.address());
    (void)connectOrFail(server, staying, "Stays");
    (void)connectOrFail(server, leaving, "Leaves");
    server.frame();
    ASSERT_EQ(server.shipCount(), 2U);

    leaving.sendReliable(LeaveGame{});
    const auto left = waitFor<PlayerLeft>(server, staying);

    ASSERT_TRUE(left.has_value());
    EXPECT_EQ(*left, (PlayerLeft{.playerId = 1, .reason = LeaveReason::Quit}));
    EXPECT_EQ(server.sessionCount(), 2U);
    EXPECT_EQ(server.shipCount(), 1U);
}

TEST(LeaveGame, NothingMoreIsSentAfterLeaving)
{
    TestServer server;
    TestClient client(server.address());
    (void)connectOrFail(server, client, "Dave");
    ASSERT_TRUE(waitFor<Snapshot>(server, client).has_value());

    client.sendReliable(LeaveGame{});
    for (int i = 0; i < 20; ++i) {
        std::this_thread::sleep_for(1ms);
        server.frame(1.0F / 60.0F);
    }
    client.poll();
    client.inbox.clear(); // What was on its way when the server read LEAVE_GAME

    for (int i = 0; i < 20; ++i) {
        server.frame(0.05F);
        std::this_thread::sleep_for(1ms);
        client.poll();
    }
    EXPECT_FALSE(client.take<Snapshot>().has_value());
    EXPECT_FALSE(client.take<EntitySpawn>().has_value());
    EXPECT_FALSE(client.take<GameState>().has_value());
}

TEST(LeaveGame, OnTheMainPageItIsIgnored)
{
    TestServer server;
    TestClient player(server.address());
    (void)connectOrFail(server, player, "Dave");
    TestClient waiting(server.address());
    logInOrFail(server, waiting, "Eve");

    waiting.sendReliable(LeaveGame{});

    EXPECT_FALSE(waitFor<PlayerLeft>(server, player).has_value());
    EXPECT_EQ(server.sessionCount(), 2U);
}

TEST(LeaveGame, ComingBackGivesEverythingAgain)
{
    TestServer server;
    TestClient client(server.address());
    (void)connectOrFail(server, client, "Dave");
    ASSERT_TRUE(waitFor<GameState>(server, client).has_value());

    client.sendReliable(LeaveGame{});
    deliver(server);
    server.frame();
    client.poll();
    client.inbox.clear();

    EXPECT_EQ(joinOrFail(server, client), (JoinResult{.code = JoinCode::Ok, .playerId = 0}));
    const auto spawn = waitFor<EntitySpawn>(server, client);
    const auto state = waitFor<GameState>(server, client);
    EXPECT_TRUE(spawn.has_value());
    EXPECT_TRUE(state.has_value());
    EXPECT_EQ(server.shipCount(), 1U);
}

TEST(LeaveGame, TheNextGameStartsWithFreshInputs)
{
    TestServer server;
    TestClient client(server.address());
    (void)connectOrFail(server, client, "Dave");
    client.send(Input{.firstTick = 1000, .count = 1, .masks = {input::Right}});
    deliver(server);
    ASSERT_NE(server.playerInput(0), nullptr);

    client.sendReliable(LeaveGame{});
    deliver(server);
    EXPECT_EQ(server.playerInput(0), nullptr);

    EXPECT_EQ(joinOrFail(server, client).code, JoinCode::Ok);
    client.send(Input{.firstTick = 1, .count = 1, .masks = {input::Up}}); // Lower tick
    deliver(server);

    const server::PlayerInput* input = server.playerInput(0);
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(input->mask, input::Up);
}

// --- Friends (§8.5) ---------------------------------------------------------------

namespace {

/// The next FRIEND_ENTRY about `name`, if one comes.
std::optional<FriendEntry> entryAbout(
    TestServer& server, TestClient& client, const std::string& name)
{
    while (const auto entry = waitFor<FriendEntry>(server, client)) {
        if (entry->name == name) {
            return entry;
        }
    }
    return std::nullopt;
}

/// Two players logged in, friends, each already told about the other.
struct TwoFriends {
    explicit TwoFriends(TestServer& server)
        : ann(server.address())
        , ben(server.address())
    {
        logInOrFail(server, ann, "Ann");
        logInOrFail(server, ben, "Ben");
        ann.sendReliable(FriendRequest{.name = "Ben"});
        EXPECT_TRUE(entryAbout(server, ben, "Ann").has_value());
        ben.sendReliable(FriendAnswer{.name = "Ann", .reply = FriendReply::Accept});
        const auto forBen = entryAbout(server, ben, "Ann");
        EXPECT_TRUE(forBen.has_value());
        EXPECT_EQ(forBen.value_or(FriendEntry{}).state, FriendState::Online);
    }

    TestClient ann;
    TestClient ben;
};

} // namespace

TEST(Friends, RequestReachesTheOtherPlayer)
{
    TestServer server;
    TestClient ann(server.address());
    TestClient ben(server.address());
    logInOrFail(server, ann, "Ann");
    logInOrFail(server, ben, "Ben");

    ann.sendReliable(FriendRequest{.name = "Ben"});

    const auto result = waitFor<FriendResult>(server, ann);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->code, FriendCode::Ok);
    EXPECT_EQ(waitFor<FriendEntry>(server, ann),
        (FriendEntry{.name = "Ben", .state = FriendState::RequestSent}));
    EXPECT_EQ(waitFor<FriendEntry>(server, ben),
        (FriendEntry{.name = "Ann", .state = FriendState::RequestReceived}));
}

TEST(Friends, UnknownNameIsRefused)
{
    TestServer server;
    TestClient ann(server.address());
    logInOrFail(server, ann, "Ann");

    ann.sendReliable(FriendRequest{.name = "Nobody"});

    EXPECT_EQ(waitFor<FriendResult>(server, ann), (FriendResult{.code = FriendCode::UnknownName}));
}

TEST(Friends, OwnNameIsRefused)
{
    TestServer server;
    TestClient ann(server.address());
    logInOrFail(server, ann, "Ann");

    ann.sendReliable(FriendRequest{.name = "Ann"});

    EXPECT_EQ(waitFor<FriendResult>(server, ann), (FriendResult{.code = FriendCode::Yourself}));
}

TEST(Friends, AskingTwiceIsRefused)
{
    TestServer server;
    TestClient ann(server.address());
    TestClient ben(server.address());
    logInOrFail(server, ann, "Ann");
    logInOrFail(server, ben, "Ben");
    ann.sendReliable(FriendRequest{.name = "Ben"});
    ASSERT_TRUE(waitFor<FriendResult>(server, ann).has_value());

    ann.sendReliable(FriendRequest{.name = "Ben"});

    EXPECT_EQ(
        waitFor<FriendResult>(server, ann), (FriendResult{.code = FriendCode::AlreadyLinked}));
}

TEST(Friends, AcceptingTellsBothAndShowsWhoIsOnline)
{
    TestServer server;
    TwoFriends both(server);

    const auto forAnn = entryAbout(server, both.ann, "Ben");
    ASSERT_TRUE(forAnn.has_value()); // RequestSent, read first
    const auto then = entryAbout(server, both.ann, "Ben");
    ASSERT_TRUE(then.has_value());
    EXPECT_EQ(then->state, FriendState::Online);
}

TEST(Friends, RefusingForgetsTheRequestOnBothSides)
{
    TestServer server;
    TestClient ann(server.address());
    TestClient ben(server.address());
    logInOrFail(server, ann, "Ann");
    logInOrFail(server, ben, "Ben");
    ann.sendReliable(FriendRequest{.name = "Ben"});
    ASSERT_TRUE(entryAbout(server, ben, "Ann").has_value());
    ASSERT_TRUE(entryAbout(server, ann, "Ben").has_value());

    ben.sendReliable(FriendAnswer{.name = "Ann", .reply = FriendReply::Refuse});

    EXPECT_EQ(entryAbout(server, ann, "Ben"),
        (FriendEntry{.name = "Ben", .state = FriendState::Removed}));
    EXPECT_EQ(entryAbout(server, ben, "Ann"),
        (FriendEntry{.name = "Ann", .state = FriendState::Removed}));
}

TEST(Friends, AnsweringNoRequestDoesNothing)
{
    // Nobody can make themselves someone's friend by "accepting" a request never sent
    TestServer server;
    TestClient ann(server.address());
    TestClient ben(server.address());
    logInOrFail(server, ann, "Ann");
    logInOrFail(server, ben, "Ben");

    ben.sendReliable(FriendAnswer{.name = "Ann", .reply = FriendReply::Accept});

    EXPECT_FALSE(waitFor<FriendEntry>(server, ben).has_value());
    EXPECT_FALSE(waitFor<FriendEntry>(server, ann).has_value());
}

TEST(Friends, AskingBackMakesThemFriends)
{
    TestServer server;
    TestClient ann(server.address());
    TestClient ben(server.address());
    logInOrFail(server, ann, "Ann");
    logInOrFail(server, ben, "Ben");
    ann.sendReliable(FriendRequest{.name = "Ben"});
    ASSERT_TRUE(entryAbout(server, ben, "Ann").has_value());

    ben.sendReliable(FriendRequest{.name = "Ann"});

    EXPECT_EQ(
        entryAbout(server, ben, "Ann"), (FriendEntry{.name = "Ann", .state = FriendState::Online}));
}

TEST(Friends, RemovingTellsBoth)
{
    TestServer server;
    TwoFriends both(server);

    both.ann.sendReliable(FriendRemove{.name = "Ben"});

    EXPECT_EQ(entryAbout(server, both.ben, "Ann"),
        (FriendEntry{.name = "Ann", .state = FriendState::Removed}));
}

TEST(Friends, FriendsLearnWhenSomeoneLeavesAndComesBack)
{
    TestServer server;
    TwoFriends both(server);

    both.ben.send(Disconnect{.reason = DisconnectReason::Quit});
    EXPECT_EQ(entryAbout(server, both.ann, "Ben"),
        (FriendEntry{.name = "Ben", .state = FriendState::RequestSent})); // Older entry
    EXPECT_EQ(entryAbout(server, both.ann, "Ben"),
        (FriendEntry{.name = "Ben", .state = FriendState::Online}));
    EXPECT_EQ(entryAbout(server, both.ann, "Ben"),
        (FriendEntry{.name = "Ben", .state = FriendState::Offline}));

    TestClient back(server.address());
    (void)openSessionOrFail(server, back, "Ben");
    EXPECT_EQ(authenticate(server, back, AuthMode::Login), AuthCode::Ok);

    EXPECT_EQ(entryAbout(server, back, "Ann"),
        (FriendEntry{.name = "Ann", .state = FriendState::Online}));
    EXPECT_EQ(entryAbout(server, both.ann, "Ben"),
        (FriendEntry{.name = "Ben", .state = FriendState::Online}));
}

TEST(Friends, ARequestWaitsForAnOfflinePlayer)
{
    TestServer server;
    TestClient ann(server.address());
    TestClient ben(server.address());
    logInOrFail(server, ann, "Ann");
    logInOrFail(server, ben, "Ben");
    ben.send(Disconnect{.reason = DisconnectReason::Quit});
    deliver(server);
    deliver(server);

    ann.sendReliable(FriendRequest{.name = "Ben"});
    ASSERT_TRUE(waitFor<FriendResult>(server, ann).has_value());

    TestClient back(server.address());
    (void)openSessionOrFail(server, back, "Ben");
    EXPECT_EQ(authenticate(server, back, AuthMode::Login), AuthCode::Ok);
    EXPECT_EQ(entryAbout(server, back, "Ann"),
        (FriendEntry{.name = "Ann", .state = FriendState::RequestReceived}));
}

TEST(Friends, TheListStaysUpToDateDuringAGame)
{
    TestServer server;
    TwoFriends both(server);
    both.ann.inbox.clear();
    ASSERT_EQ(joinOrFail(server, both.ann).code, JoinCode::Ok);

    both.ben.send(Disconnect{.reason = DisconnectReason::Quit});

    bool told = false;
    while (const auto entry = entryAbout(server, both.ann, "Ben")) {
        told = told || entry->state == FriendState::Offline;
    }
    EXPECT_TRUE(told);
}

TEST(Friends, BeforeLoggingInEverythingIsIgnored)
{
    TestServer server;
    TestClient ann(server.address());
    logInOrFail(server, ann, "Ann");
    TestClient stranger(server.address());
    (void)openSessionOrFail(server, stranger, "Stranger");

    stranger.sendReliable(FriendRequest{.name = "Ann"});

    EXPECT_FALSE(waitFor<FriendEntry>(server, ann).has_value());
    EXPECT_FALSE(waitFor<FriendResult>(server, stranger).has_value());
}

TEST(Friends, TheFileIsWrittenAtMostOncePerSecond)
{
    // Otherwise a client could make the whole game wait for the disk, 120 times a second
    TestServer server;
    TestClient ann(server.address());
    TestClient ben(server.address());
    logInOrFail(server, ann, "Ann");
    logInOrFail(server, ben, "Ben");
    server.frame(2.0F); // Long after the last save
    const auto contentOf = [&] {
        std::ifstream input(server.friendsFile(), std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input), {});
    };
    const std::string empty = contentOf();

    ann.sendReliable(FriendRequest{.name = "Ben"});
    deliver(server);
    const std::string first = contentOf();
    EXPECT_NE(first, empty); // The first change after a long time is saved at once

    ann.sendReliable(FriendRemove{.name = "Ben"});
    deliver(server);
    EXPECT_EQ(contentOf(), first); // Too soon: it waits

    server.frame(1.1F);
    EXPECT_EQ(contentOf(), empty);
}

// --- Chat (§8.6) ------------------------------------------------------------------

namespace {

/// Two friends on the main page, with nothing left to read about each other.
struct TwoChatters {
    explicit TwoChatters(TestServer& server)
        : both(server)
    {
        (void)entryAbout(server, both.ann, "Ben");
        (void)entryAbout(server, both.ann, "Ben");
        both.ann.inbox.clear();
        both.ben.inbox.clear();
    }

    TwoFriends both;
};

/// The next chat line, if one comes.
std::optional<ChatMessage> chatLine(TestServer& server, TestClient& client)
{
    return waitFor<ChatMessage>(server, client);
}

} // namespace

TEST(Chat, AMessageReachesEveryPlayerOfTheGameUnderTheSendersName)
{
    TestServer server;
    TwoChatters chatters(server);
    TestClient& ann = chatters.both.ann;
    TestClient& ben = chatters.both.ben;
    ASSERT_EQ(joinOrFail(server, ann).code, JoinCode::Ok);
    ASSERT_EQ(joinOrFail(server, ben).code, JoinCode::Ok);

    ann.sendReliable(ChatSend{.text = "hello"});

    const ChatMessage expected{.kind = ChatKind::Game, .name = "Ann", .text = "hello"};
    EXPECT_EQ(chatLine(server, ann), expected); // Her own line comes back too
    EXPECT_EQ(chatLine(server, ben), expected);
}

TEST(Chat, TheGameChatStaysInTheGame)
{
    TestServer server;
    TwoChatters chatters(server);
    TestClient& ann = chatters.both.ann;
    TestClient& ben = chatters.both.ben;
    ASSERT_EQ(joinOrFail(server, ann).code, JoinCode::Ok);

    ann.sendReliable(ChatSend{.text = "hello"});

    EXPECT_TRUE(chatLine(server, ann).has_value());
    EXPECT_FALSE(chatLine(server, ben).has_value());
}

TEST(Chat, OutsideAGameTheGameChatGoesNowhere)
{
    TestServer server;
    TwoChatters chatters(server);

    chatters.both.ann.sendReliable(ChatSend{.text = "hello"});

    EXPECT_FALSE(chatLine(server, chatters.both.ben).has_value());
    EXPECT_FALSE(chatLine(server, chatters.both.ann).has_value());
}

TEST(Chat, APrivateMessageReachesOnlyTheFriend)
{
    TestServer server;
    TwoChatters chatters(server);
    TestClient cid(server.address());
    logInOrFail(server, cid, "Cid");

    chatters.both.ann.sendReliable(PrivateSend{.name = "Ben", .text = "psst"});

    EXPECT_EQ(chatLine(server, chatters.both.ben),
        (ChatMessage{.kind = ChatKind::Received, .name = "Ann", .text = "psst"}));
    EXPECT_EQ(chatLine(server, chatters.both.ann),
        (ChatMessage{.kind = ChatKind::Sent, .name = "Ben", .text = "psst"}));
    EXPECT_FALSE(chatLine(server, cid).has_value());
}

TEST(Chat, APrivateMessageWorksDuringAGame)
{
    TestServer server;
    TwoChatters chatters(server);
    ASSERT_EQ(joinOrFail(server, chatters.both.ben).code, JoinCode::Ok);

    chatters.both.ann.sendReliable(PrivateSend{.name = "Ben", .text = "psst"});

    EXPECT_EQ(chatLine(server, chatters.both.ben),
        (ChatMessage{.kind = ChatKind::Received, .name = "Ann", .text = "psst"}));
}

TEST(Chat, OnlyFriendsCanWritePrivately)
{
    TestServer server;
    TestClient ann(server.address());
    TestClient ben(server.address());
    logInOrFail(server, ann, "Ann");
    logInOrFail(server, ben, "Ben");

    ann.sendReliable(PrivateSend{.name = "Ben", .text = "psst"});

    EXPECT_EQ(waitFor<ChatRefused>(server, ann), (ChatRefused{.reason = ChatRefusal::NotFriend}));
    EXPECT_FALSE(chatLine(server, ben).has_value());
}

TEST(Chat, AnOfflineFriendReadsItWhenComingBack)
{
    TestServer server;
    TwoChatters chatters(server);
    TestClient& ann = chatters.both.ann;
    chatters.both.ben.send(Disconnect{.reason = DisconnectReason::Quit});
    deliver(server);
    deliver(server);

    ann.sendReliable(PrivateSend{.name = "Ben", .text = "one"});
    EXPECT_EQ(
        chatLine(server, ann), (ChatMessage{.kind = ChatKind::Sent, .name = "Ben", .text = "one"}));
    ann.sendReliable(PrivateSend{.name = "Ben", .text = "two"});
    EXPECT_TRUE(chatLine(server, ann).has_value());

    TestClient back(server.address());
    (void)openSessionOrFail(server, back, "Ben");
    EXPECT_EQ(authenticate(server, back, AuthMode::Login), AuthCode::Ok);

    EXPECT_EQ(chatLine(server, back),
        (ChatMessage{.kind = ChatKind::Received, .name = "Ann", .text = "one"}));
    EXPECT_EQ(chatLine(server, back),
        (ChatMessage{.kind = ChatKind::Received, .name = "Ann", .text = "two"}));
}

TEST(Chat, WritingTooFastIsRefusedThenAllowedAgain)
{
    TestServer server;
    TwoChatters chatters(server);
    TestClient& ann = chatters.both.ann;
    for (std::size_t i = 0; i < server::ChatSystem::MaxMessagesInWindow; ++i) {
        ann.sendReliable(PrivateSend{.name = "Ben", .text = "m" + std::to_string(i)});
    }
    for (std::size_t i = 0; i < server::ChatSystem::MaxMessagesInWindow; ++i) {
        EXPECT_TRUE(chatLine(server, chatters.both.ben).has_value()) << i;
    }

    ann.sendReliable(PrivateSend{.name = "Ben", .text = "one too many"});
    EXPECT_EQ(waitFor<ChatRefused>(server, ann), (ChatRefused{.reason = ChatRefusal::TooFast}));
    EXPECT_FALSE(chatLine(server, chatters.both.ben).has_value());

    for (int second = 0; second < 6; ++second) { // A whole window, the sessions staying alive
        ann.poll();
        chatters.both.ben.poll();
        ann.send(Heartbeat{});
        chatters.both.ben.send(Heartbeat{});
        deliver(server, 1.0F);
    }
    ann.sendReliable(PrivateSend{.name = "Ben", .text = "later"});
    EXPECT_EQ(chatLine(server, chatters.both.ben),
        (ChatMessage{.kind = ChatKind::Received, .name = "Ann", .text = "later"}));
}

TEST(Chat, BeforeLoggingInEverythingIsIgnored)
{
    TestServer server;
    TwoChatters chatters(server);
    TestClient stranger(server.address());
    (void)openSessionOrFail(server, stranger, "Stranger");

    stranger.sendReliable(PrivateSend{.name = "Ann", .text = "psst"});

    EXPECT_FALSE(chatLine(server, chatters.both.ann).has_value());
    EXPECT_FALSE(waitFor<ChatRefused>(server, stranger).has_value());
}

TEST(Chat, NoTextIsReadableOnTheNetwork)
{
    TestServer server;
    TwoChatters chatters(server);
    TestClient& ann = chatters.both.ann;
    TestClient& ben = chatters.both.ben;
    ben.rawDatagrams.clear();

    ann.sendReliable(PrivateSend{.name = "Ben", .text = "my secret words"});
    ASSERT_TRUE(chatLine(server, ben).has_value());

    const std::string secret = "my secret words";
    for (const auto& datagram : ben.rawDatagrams) {
        const std::string text(reinterpret_cast<const char*>(datagram.data()), datagram.size());
        EXPECT_EQ(text.find(secret), std::string::npos);
    }
}

TEST(Chat, FromTheMainPageNobodyInTheGameIsReached)
{
    TestServer server;
    TwoChatters chatters(server);
    ASSERT_EQ(joinOrFail(server, chatters.both.ben).code, JoinCode::Ok);

    chatters.both.ann.sendReliable(ChatSend{.text = "hello"});

    EXPECT_FALSE(chatLine(server, chatters.both.ben).has_value());
}

TEST(Chat, TheLimitSlidesWithTime)
{
    // 1 message, 4 seconds, 4 messages, 2 seconds: the first left the window, the others not
    TestServer server;
    TwoChatters chatters(server);
    TestClient& ann = chatters.both.ann;
    TestClient& ben = chatters.both.ben;
    const auto waitSeconds = [&](int seconds) {
        for (int second = 0; second < seconds; ++second) {
            ann.poll();
            ben.poll();
            ann.send(Heartbeat{});
            ben.send(Heartbeat{});
            deliver(server, 1.0F);
        }
    };
    ann.sendReliable(PrivateSend{.name = "Ben", .text = "first"});
    ASSERT_TRUE(chatLine(server, ben).has_value());
    waitSeconds(4);
    for (int i = 0; i < 4; ++i) {
        ann.sendReliable(PrivateSend{.name = "Ben", .text = "next"});
    }
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(chatLine(server, ben).has_value()) << i;
    }
    waitSeconds(2);

    ann.sendReliable(PrivateSend{.name = "Ben", .text = "allowed"});
    EXPECT_EQ(chatLine(server, ben),
        (ChatMessage{.kind = ChatKind::Received, .name = "Ann", .text = "allowed"}));

    ann.sendReliable(PrivateSend{.name = "Ben", .text = "too much"});
    EXPECT_EQ(waitFor<ChatRefused>(server, ann), (ChatRefused{.reason = ChatRefusal::TooFast}));
}

TEST(Chat, TheFileIsWrittenAtMostOncePerSecond)
{
    TestServer server;
    TwoChatters chatters(server);
    TestClient& ann = chatters.both.ann;
    chatters.both.ben.send(Disconnect{.reason = DisconnectReason::Quit});
    deliver(server);
    ann.send(Heartbeat{});
    server.frame(2.0F); // Long after the last save
    const auto contentOf = [&] {
        std::ifstream input(server.messagesFile(), std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input), {});
    };

    ann.sendReliable(PrivateSend{.name = "Ben", .text = "first"});
    deliver(server);
    const std::string first = contentOf();
    EXPECT_NE(first.find("first"), std::string::npos); // Saved at once

    ann.sendReliable(PrivateSend{.name = "Ben", .text = "second"});
    deliver(server);
    EXPECT_EQ(contentOf(), first); // Too soon: it waits

    server.frame(1.1F);
    EXPECT_NE(contentOf().find("second"), std::string::npos);
}
