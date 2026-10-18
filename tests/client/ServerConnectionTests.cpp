#include "client/Network/ServerConnection.hpp"
#include "network/Protocol/PacketCodec.hpp"
#include "network/Reliability/ReliableChannel.hpp"
#include "network/Transport/UdpSocket.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <span>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

using namespace network::protocol;
using namespace std::chrono_literals;
using Status = client::ServerConnection::Status;
using Kind = client::ServerConnection::Failure::Kind;

namespace {

/// A server played by the test on 127.0.0.1, with its own reliable channel.
struct FakeServer {
    network::UdpSocket socket{*network::UdpSocket::open(0)};
    network::ReliableChannel channel{0x1234ABCD};
    network::Endpoint client;

    [[nodiscard]]
    std::uint16_t port() const
    {
        return socket.localEndpoint()->port();
    }

    /// Waits a little for the next packet from the client (loopback is fast).
    std::optional<Packet> receive()
    {
        DatagramBuffer buffer{};
        for (int tries = 0; tries < 200; ++tries) {
            auto received = socket.receive(buffer);
            if (received && *received) {
                client = (*received)->from;
                auto packet
                    = decode(std::span(buffer).first((*received)->size), Direction::ClientToServer);
                if (packet) {
                    return *packet;
                }
            }
            std::this_thread::sleep_for(1ms);
        }
        return std::nullopt;
    }

    void send(const Packet& packet)
    {
        DatagramBuffer buffer{};
        const auto size = encode(packet, buffer);
        ASSERT_TRUE(size.has_value());
        ASSERT_TRUE(socket.sendTo(std::span(buffer).first(*size), client).has_value());
    }

    /// Answers the first CONNECT_REQUEST with CONNECT_ACCEPT (player 2).
    void accept(client::ServerConnection::Clock::time_point now)
    {
        const auto request = receive();
        ASSERT_TRUE(request.has_value());
        [[maybe_unused]] const auto delivered = channel.receive(*request, now);
        send(channel.wrap(
            ConnectAccept{.playerId = 2, .tickRate = 60, .snapshotRate = 30, .maxPlayers = 4},
            now));
    }
};

/// Reads the client's socket until something changes (or 200 ms pass).
std::vector<Payload> receiveSoon(
    client::ServerConnection& connection, client::ServerConnection::Clock::time_point now)
{
    const Status before = connection.status();
    for (int tries = 0; tries < 200; ++tries) {
        auto payloads = connection.receive(now);
        if (!payloads.empty() || connection.status() != before) {
            return payloads;
        }
        std::this_thread::sleep_for(1ms);
    }
    return {};
}

struct ServerConnectionTest : testing::Test {
    FakeServer server;
    client::ServerConnection connection{*client::ServerConnection::open()};
    client::ServerConnection::Clock::time_point now = client::ServerConnection::Clock::now();

    void connected()
    {
        connection.connect("127.0.0.1", server.port(), "Thibani", now);
        server.accept(now);
        static_cast<void>(receiveSoon(connection, now));
        ASSERT_EQ(connection.status(), Status::Connected);
    }
};

} // namespace

TEST_F(ServerConnectionTest, SendsConnectRequestWithTheName)
{
    connection.connect("127.0.0.1", server.port(), "Thibani", now);
    EXPECT_EQ(connection.status(), Status::Connecting);
    EXPECT_EQ(connection.attempts(), 1);

    const auto request = server.receive();
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->sessionToken, 0U); // §4.1: MUST be 0
    ASSERT_TRUE(std::holds_alternative<ConnectRequest>(request->payload));
    EXPECT_EQ(std::get<ConnectRequest>(request->payload).name, "Thibani");
}

TEST_F(ServerConnectionTest, AcceptConnectsAndGivesThePlayerId)
{
    connected();
    ASSERT_TRUE(connection.accepted().has_value());
    EXPECT_EQ(connection.accepted()->playerId, 2);

    // Every following packet carries the session token
    connection.sendInput(Input{.firstTick = 0, .count = 1, .masks = {}}, now);
    const auto input = server.receive();
    ASSERT_TRUE(input.has_value());
    EXPECT_EQ(input->sessionToken, 0x1234ABCDU);
    EXPECT_TRUE(std::holds_alternative<Input>(input->payload));
}

TEST_F(ServerConnectionTest, RejectFailsWithTheReason)
{
    connection.connect("127.0.0.1", server.port(), "Thibani", now);
    ASSERT_TRUE(server.receive().has_value());
    server.send(Packet{.sessionToken = 0,
        .sequence = 0,
        .ack = std::nullopt,
        .reliableId = 0,
        .payload = ConnectReject{.reason = RejectReason::ServerFull}});
    static_cast<void>(receiveSoon(connection, now));

    EXPECT_EQ(connection.status(), Status::Failed);
    EXPECT_EQ(connection.failure().kind, Kind::Rejected);
    EXPECT_EQ(client::describe(connection.failure()), "Server full. Try again later.");
}

TEST_F(ServerConnectionTest, RetriesEvery500msThenGivesUpAfter10)
{
    connection.connect("127.0.0.1", server.port(), "Thibani", now);
    connection.update(now + 499ms);
    EXPECT_EQ(connection.attempts(), 1); // Too early
    for (int attempt = 2; attempt <= client::ServerConnection::MaxAttempts; ++attempt) {
        now += client::ServerConnection::RetryDelay;
        connection.update(now);
        EXPECT_EQ(connection.attempts(), attempt);
    }
    connection.update(now + client::ServerConnection::RetryDelay);
    EXPECT_EQ(connection.status(), Status::Failed);
    EXPECT_EQ(connection.failure().kind, Kind::NoAnswer);

    int requests = 0;
    while (server.receive()) {
        ++requests;
    }
    EXPECT_EQ(requests, client::ServerConnection::MaxAttempts);
}

TEST_F(ServerConnectionTest, ReliableEventsArriveOnceAndInOrder)
{
    connected();
    server.channel.queueReliable(PlayerJoined{.playerId = 0, .name = "A"});
    server.channel.queueReliable(GameState{.phase = GamePhase::Running, .score = 5});
    const auto packets = server.channel.takeDue(now);
    ASSERT_EQ(packets.size(), 2U);
    server.send(packets.at(1)); // Out of order...
    server.send(packets.at(0));
    server.send(packets.at(0)); // ...and duplicated

    std::vector<Payload> received;
    for (int i = 0; i < 50 && received.size() < 2; ++i) {
        for (auto& payload : receiveSoon(connection, now)) {
            received.push_back(std::move(payload));
        }
    }
    static_cast<void>(receiveSoon(connection, now)); // The duplicate, if late
    ASSERT_EQ(received.size(), 2U);
    EXPECT_TRUE(std::holds_alternative<PlayerJoined>(received.at(0)));
    EXPECT_TRUE(std::holds_alternative<GameState>(received.at(1)));
}

TEST_F(ServerConnectionTest, IgnoresAnotherSessionToken)
{
    connected();
    Packet forged = server.channel.wrap(GameState{.phase = GamePhase::GameOver, .score = 0}, now);
    forged.sessionToken = 0xDEADBEEF;
    server.send(forged);
    EXPECT_TRUE(receiveSoon(connection, now).empty());
    EXPECT_EQ(connection.status(), Status::Connected);
}

TEST_F(ServerConnectionTest, HeartbeatAfter250msOfSilenceThenTimeoutAfter5s)
{
    connected();
    while (server.receive()) { } // Drop what was sent so far

    connection.update(now + 249ms);
    connection.update(now + client::ServerConnection::HeartbeatDelay);
    const auto heartbeat = server.receive();
    ASSERT_TRUE(heartbeat.has_value());
    EXPECT_TRUE(std::holds_alternative<Heartbeat>(heartbeat->payload));

    connection.update(now + client::ServerConnection::Timeout);
    EXPECT_EQ(connection.status(), Status::Failed);
    EXPECT_EQ(connection.failure().kind, Kind::ConnectionLost);
}

TEST_F(ServerConnectionTest, ServerDisconnectEndsTheSession)
{
    connected();
    server.send(server.channel.wrap(Disconnect{.reason = DisconnectReason::ServerShutdown}, now));
    static_cast<void>(receiveSoon(connection, now));
    EXPECT_EQ(connection.status(), Status::Failed);
    EXPECT_EQ(connection.failure().kind, Kind::Disconnected);
    EXPECT_EQ(client::describe(connection.failure()), "The server shut down.");
}

TEST_F(ServerConnectionTest, DisconnectSendsQuitThreeTimes)
{
    connected();
    while (server.receive()) { }
    connection.disconnect(now);
    EXPECT_EQ(connection.status(), Status::Idle);
    int quits = 0;
    while (const auto packet = server.receive()) {
        const auto* disconnect = std::get_if<Disconnect>(&packet->payload);
        quits += disconnect != nullptr && disconnect->reason == DisconnectReason::Quit ? 1 : 0;
    }
    EXPECT_EQ(quits, client::ServerConnection::DisconnectRepeats);
}
