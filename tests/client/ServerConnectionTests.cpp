#include "client/Network/ServerConnection.hpp"
#include "network/Crypto/SealedDatagram.hpp"
#include "network/Protocol/PacketCodec.hpp"
#include "network/Reliability/ReliableChannel.hpp"
#include "network/Transport/UdpSocket.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
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
    network::crypto::KeyPair keys = network::crypto::generateKeyPair();
    std::optional<network::crypto::SessionCipher> cipher; // Set by accept()
    std::vector<std::byte> lastSent; // The last datagram, as it went on the network

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
                std::span<const std::byte> datagram = std::span(buffer).first((*received)->size);
                DatagramBuffer opened{};
                if (cipher) {
                    const auto size = network::crypto::openDatagram(*cipher, datagram, opened);
                    if (!size) {
                        continue;
                    }
                    datagram = std::span(opened).first(*size);
                }
                auto packet = decode(datagram, Direction::ClientToServer);
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
        std::span<const std::byte> datagram = std::span(buffer).first(*size);
        DatagramBuffer sealed{};
        if (cipher) {
            const auto sealedSize = network::crypto::sealDatagram(*cipher, datagram, sealed);
            ASSERT_TRUE(sealedSize.has_value());
            datagram = std::span(sealed).first(*sealedSize);
        }
        sendBytes(datagram);
    }

    void sendBytes(std::span<const std::byte> datagram)
    {
        lastSent.assign(datagram.begin(), datagram.end());
        ASSERT_TRUE(socket.sendTo(datagram, client).has_value());
    }

    /// Answers the first CONNECT_REQUEST with CONNECT_ACCEPT.
    void accept(client::ServerConnection::Clock::time_point now)
    {
        const auto request = receive();
        ASSERT_TRUE(request.has_value());
        [[maybe_unused]] const auto delivered = channel.receive(*request, now);
        send(channel.wrap(ConnectAccept{.playerId = NoPlayer,
                              .tickRate = 60,
                              .snapshotRate = 30,
                              .maxPlayers = 4,
                              .publicKey = keys.publicKey},
            now));
        const auto key = network::crypto::deriveSessionKey(keys,
            std::get<ConnectRequest>(request->payload).publicKey, network::crypto::Role::Server);
        ASSERT_TRUE(key.has_value());
        cipher.emplace(*key, network::crypto::Role::Server); // Everything else is sealed
    }
};

const client::ServerConnection::Credentials Dave{
    .name = "Thibani", .password = "correct horse", .mode = AuthMode::Login};

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
        connection.connect("127.0.0.1", server.port(), Dave, now);
        server.accept(now);
        static_cast<void>(receiveSoon(connection, now));
        ASSERT_EQ(connection.status(), Status::LoggingIn);
        answerLogin(AuthCode::Ok);
        ASSERT_EQ(connection.status(), Status::Connected);
    }

    /// Reads the AUTH_REQUEST and answers it, reliably as the real server does.
    void answerLogin(AuthCode code)
    {
        const auto request = server.receive();
        ASSERT_TRUE(request.has_value());
        ASSERT_TRUE(std::holds_alternative<AuthRequest>(request->payload));
        [[maybe_unused]] const auto delivered = server.channel.receive(*request, now);
        server.channel.queueReliable(AuthResult{.code = code});
        for (const Packet& packet : server.channel.takeDue(now)) {
            server.send(packet);
        }
        static_cast<void>(receiveSoon(connection, now));
    }
};

} // namespace

TEST_F(ServerConnectionTest, SendsConnectRequestWithTheName)
{
    connection.connect("127.0.0.1", server.port(), Dave, now);
    EXPECT_EQ(connection.status(), Status::Connecting);
    EXPECT_EQ(connection.attempts(), 1);

    const auto request = server.receive();
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->sessionToken, 0U); // §4.1: MUST be 0
    ASSERT_TRUE(std::holds_alternative<ConnectRequest>(request->payload));
    EXPECT_EQ(std::get<ConnectRequest>(request->payload).name, "Thibani");
}

TEST_F(ServerConnectionTest, AcceptConnectsWithoutAPlaceInTheGame)
{
    connected();
    ASSERT_TRUE(connection.accepted().has_value());
    EXPECT_EQ(connection.accepted()->playerId, NoPlayer);

    // Every following packet carries the session token
    connection.sendInput(Input{.firstTick = 0, .count = 1, .masks = {}}, now);
    const auto input = server.receive();
    ASSERT_TRUE(input.has_value());
    EXPECT_EQ(input->sessionToken, 0x1234ABCDU);
    EXPECT_TRUE(std::holds_alternative<Input>(input->payload));
}

TEST_F(ServerConnectionTest, RejectFailsWithTheReason)
{
    connection.connect("127.0.0.1", server.port(), Dave, now);
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
    connection.connect("127.0.0.1", server.port(), Dave, now);
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

TEST_F(ServerConnectionTest, RetriesKeepTheSameKeyAndANewConnectionGetsANewOne)
{
    connection.connect("127.0.0.1", server.port(), Dave, now);
    const auto first = server.receive();
    connection.update(now + client::ServerConnection::RetryDelay);
    const auto retry = server.receive();
    connection.connect("127.0.0.1", server.port(), Dave, now);
    const auto second = server.receive();
    ASSERT_TRUE(first.has_value() && retry.has_value() && second.has_value());

    const auto keyOf
        = [](const Packet& packet) { return std::get<ConnectRequest>(packet.payload).publicKey; };
    EXPECT_EQ(keyOf(*first), keyOf(*retry));
    EXPECT_NE(keyOf(*first), keyOf(*second));
}

TEST_F(ServerConnectionTest, InvalidServerKeyFailsTheConnection)
{
    connection.connect("127.0.0.1", server.port(), Dave, now);
    ASSERT_TRUE(server.receive().has_value());

    server.send(server.channel.wrap(
        ConnectAccept{.playerId = NoPlayer, .tickRate = 60, .snapshotRate = 30, .maxPlayers = 4},
        now));
    static_cast<void>(receiveSoon(connection, now));

    EXPECT_EQ(connection.status(), Status::Failed);
    EXPECT_EQ(connection.failure().kind, Kind::ProtocolError);
}

TEST_F(ServerConnectionTest, PacketInClearIsIgnoredOnceConnected)
{
    connected();
    DatagramBuffer buffer{};
    const auto size = encode( // The right token, but not sealed
        server.channel.wrap(Disconnect{.reason = DisconnectReason::Kicked}, now), buffer);
    ASSERT_TRUE(size.has_value());
    server.sendBytes(std::span(buffer).first(*size));

    EXPECT_TRUE(receiveSoon(connection, now).empty());
    EXPECT_EQ(connection.status(), Status::Connected);
}

TEST_F(ServerConnectionTest, ModifiedPacketIsIgnored)
{
    connected();
    DatagramBuffer plain{};
    const auto size
        = encode(server.channel.wrap(Disconnect{.reason = DisconnectReason::Kicked}, now), plain);
    ASSERT_TRUE(size.has_value());
    DatagramBuffer sealed{};
    const auto sealedSize
        = network::crypto::sealDatagram(*server.cipher, std::span(plain).first(*size), sealed);
    ASSERT_TRUE(sealedSize.has_value());
    sealed.at(*sealedSize - 1) ^= std::byte{0x01};

    server.sendBytes(std::span(sealed).first(*sealedSize));

    EXPECT_TRUE(receiveSoon(connection, now).empty());
    EXPECT_EQ(connection.status(), Status::Connected) << "a forged DISCONNECT kicked the player";
}

TEST_F(ServerConnectionTest, ReplayedPacketIsIgnored)
{
    connected();
    server.send(server.channel.wrap(
        Snapshot{
            .serverTick = 1, .lastInputTick = 0, .partIndex = 0, .partCount = 1, .entities = {}},
        now));
    const auto datagram = server.lastSent;
    EXPECT_EQ(receiveSoon(connection, now).size(), 1U);

    // Sent again: exactly the same bytes, so the same counter
    server.sendBytes(datagram);
    EXPECT_TRUE(receiveSoon(connection, now).empty());
}

TEST_F(ServerConnectionTest, InputIsNotReadableOnTheNetwork)
{
    connected();
    while (server.receive()) { }
    const auto cipher = server.cipher;
    server.cipher.reset(); // As an attacker: no key

    connection.sendInput(Input{.firstTick = 7, .count = 1, .masks = {}}, now);

    EXPECT_FALSE(server.receive().has_value()) << "an input can be decoded without the key";
    server.cipher = cipher;
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

// --- Login ----------------------------------------------------------------------

TEST_F(ServerConnectionTest, LogsInWithTheCredentialsOnceEncrypted)
{
    connection.connect("127.0.0.1", server.port(), Dave, now);
    server.accept(now);
    static_cast<void>(receiveSoon(connection, now));
    EXPECT_EQ(connection.status(), Status::LoggingIn);

    const auto request = server.receive();

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(std::get<AuthRequest>(request->payload),
        (AuthRequest{.mode = AuthMode::Login, .password = "correct horse"}));
}

TEST_F(ServerConnectionTest, RetriesTheLoginEvery500ms)
{
    connection.connect("127.0.0.1", server.port(), Dave, now);
    server.accept(now);
    static_cast<void>(receiveSoon(connection, now));
    ASSERT_TRUE(server.receive().has_value());

    connection.update(now + 499ms);
    EXPECT_EQ(connection.attempts(), 1);
    connection.update(now + client::ServerConnection::RetryDelay);
    EXPECT_EQ(connection.attempts(), 2);

    const auto retry = server.receive();
    ASSERT_TRUE(retry.has_value());
    EXPECT_TRUE(std::holds_alternative<AuthRequest>(retry->payload));
}

TEST_F(ServerConnectionTest, RefusedLoginReportsWhyAndFreesTheSlot)
{
    connection.connect("127.0.0.1", server.port(), Dave, now);
    server.accept(now);
    static_cast<void>(receiveSoon(connection, now));

    answerLogin(AuthCode::WrongCredentials);

    EXPECT_EQ(connection.status(), Status::Failed);
    EXPECT_EQ(connection.failure().kind, Kind::AuthFailed);
    EXPECT_EQ(client::describe(connection.failure()), "Wrong name or password.");
    const auto bye = server.receive();
    ASSERT_TRUE(bye.has_value());
    EXPECT_TRUE(std::holds_alternative<Disconnect>(bye->payload));
}

TEST_F(ServerConnectionTest, EventsAfterTheLoginReachTheGameEvenInTheSameBatch)
{
    connection.connect("127.0.0.1", server.port(), Dave, now);
    server.accept(now);
    static_cast<void>(receiveSoon(connection, now));

    server.channel.queueReliable(AuthResult{.code = AuthCode::Ok});
    server.channel.queueReliable(PlayerJoined{.playerId = 0, .name = "A"});
    const auto packets = server.channel.takeDue(now);
    ASSERT_EQ(packets.size(), 2U);
    server.send(packets.at(1)); // The event overtakes AUTH_RESULT on the network
    server.send(packets.at(0));

    std::vector<Payload> received;
    for (int i = 0; i < 200 && received.empty(); ++i) {
        received = connection.receive(now);
        std::this_thread::sleep_for(1ms);
    }

    EXPECT_EQ(connection.status(), Status::Connected);
    ASSERT_EQ(received.size(), 1U);
    EXPECT_TRUE(std::holds_alternative<PlayerJoined>(received.at(0)));
}

TEST_F(ServerConnectionTest, SnapshotBeforeTheLoginResultIsIgnored)
{
    connection.connect("127.0.0.1", server.port(), Dave, now);
    server.accept(now);
    static_cast<void>(receiveSoon(connection, now));

    server.send(server.channel.wrap(Snapshot{}, now));

    EXPECT_TRUE(receiveSoon(connection, now).empty());
    EXPECT_EQ(connection.status(), Status::LoggingIn);
}

TEST_F(ServerConnectionTest, NoInputBeforeTheLoginResult)
{
    connection.connect("127.0.0.1", server.port(), Dave, now);
    server.accept(now);
    static_cast<void>(receiveSoon(connection, now));
    ASSERT_TRUE(server.receive().has_value());

    connection.sendInput(Input{.firstTick = 1, .count = 1, .masks = {}}, now);

    EXPECT_FALSE(server.receive().has_value());
}

TEST_F(ServerConnectionTest, ReliableMessageIsResentUntilAcknowledged)
{
    connected();

    connection.sendReliable(JoinGame{});
    connection.update(now);
    std::this_thread::sleep_for(20ms);
    const auto first = server.receive();
    ASSERT_TRUE(first.has_value());
    EXPECT_TRUE(std::holds_alternative<JoinGame>(first->payload));

    connection.update(now + client::ServerConnection::Clock::duration{1}); // Too soon
    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(server.receive().has_value());

    const auto later = now + network::ReliableChannel::MinResendDelay;
    connection.update(later);
    std::this_thread::sleep_for(20ms);
    const auto second = server.receive();
    ASSERT_TRUE(second.has_value());
    EXPECT_TRUE(std::holds_alternative<JoinGame>(second->payload));
    EXPECT_EQ(second->reliableId, first->reliableId);
}

TEST_F(ServerConnectionTest, ReliableMessageBeforeLoggingInIsNotSent)
{
    connection.connect("127.0.0.1", server.port(), Dave, now);
    server.accept(now);
    static_cast<void>(receiveSoon(connection, now));
    ASSERT_EQ(connection.status(), Status::LoggingIn);

    connection.sendReliable(JoinGame{});
    connection.update(now);
    std::this_thread::sleep_for(20ms);

    while (const auto packet = server.receive()) {
        EXPECT_FALSE(std::holds_alternative<JoinGame>(packet->payload));
    }
}
