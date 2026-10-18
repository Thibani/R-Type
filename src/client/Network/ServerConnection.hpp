#pragma once

#include "network/Protocol/Packets.hpp"
#include "network/Reliability/ReliableChannel.hpp"
#include "network/Transport/UdpSocket.hpp"

#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace client {

/// The client's side of one session with the server (docs/protocol.md §8).
///
/// It owns the UDP socket and the session's `ReliableChannel`, and only knows
/// packets: no ECS, no graphics. It never reads a clock either: every call
/// takes the current time, so tests control it.
///
///     auto connection = ServerConnection::open();
///     connection->connect("127.0.0.1", 4242, "Thibani", now);
///     // each frame:
///     for (const Payload& payload : connection->receive(now)) { /* game events */ }
///     connection->sendInput(input, now);
///     connection->update(now); // retries, heartbeat, timeout
class ServerConnection {
public:
    using Clock = std::chrono::steady_clock;

    static constexpr std::chrono::milliseconds RetryDelay{500}; // §8.1
    static constexpr int MaxAttempts = 10; // §8.1
    static constexpr std::chrono::milliseconds HeartbeatDelay{250}; // §8.2
    static constexpr std::chrono::seconds Timeout{5}; // §8.2
    static constexpr int DisconnectRepeats = 3; // §4.1: best effort

    enum class Status : std::uint8_t {
        Idle, // Not connected, nothing to do
        Connecting, // CONNECT_REQUEST sent, waiting for an answer
        Connected, // CONNECT_ACCEPT received: in the game
        Failed, // Gave up: see failure()
    };

    /// Why the last session ended badly.
    struct Failure {
        enum class Kind : std::uint8_t {
            BadAddress, // The server address could not be resolved
            NoAnswer, // 10 requests, no answer
            Rejected, // CONNECT_REJECT: see `reject`
            ConnectionLost, // Nothing received for 5 seconds
            Disconnected, // DISCONNECT from the server: see `disconnect`
            ProtocolError, // The server broke the protocol
        };
        Kind kind{Kind::NoAnswer};
        network::protocol::RejectReason reject{network::protocol::RejectReason::ServerFull};
        network::protocol::DisconnectReason disconnect{
            network::protocol::DisconnectReason::ServerShutdown};
    };

    /// Opens a UDP socket on a free local port.
    [[nodiscard]]
    static std::expected<ServerConnection, std::error_code> open();

    /// Starts a new session: resolves `host` (IPv4 address or name) and sends
    /// the first CONNECT_REQUEST. Fails with `BadAddress` if it cannot resolve.
    void connect(
        std::string_view host, std::uint16_t port, std::string name, Clock::time_point now);

    /// Reads every pending datagram from the server. Handles the session
    /// packets itself (ACCEPT, REJECT, DISCONNECT, HEARTBEAT) and returns the
    /// game payloads, in order and without duplicates.
    [[nodiscard]]
    std::vector<network::protocol::Payload> receive(Clock::time_point now);

    /// Sends what is due: CONNECT_REQUEST retries, a HEARTBEAT after 250 ms
    /// of silence. Detects the 5 s timeout.
    void update(Clock::time_point now);

    /// Sends the player's input (connected only).
    void sendInput(const network::protocol::Input& input, Clock::time_point now);

    /// Sends DISCONNECT (QUIT) 3 times if connected, then forgets the session.
    void disconnect(Clock::time_point now);

    [[nodiscard]]
    Status status() const noexcept;
    /// Number of CONNECT_REQUEST sent for the current attempt (1 to 10).
    [[nodiscard]]
    int attempts() const noexcept;
    /// What the server said in CONNECT_ACCEPT (connected only).
    [[nodiscard]]
    const std::optional<network::protocol::ConnectAccept>& accepted() const noexcept;
    [[nodiscard]]
    const Failure& failure() const noexcept;
    /// Port of the local socket (for tests).
    [[nodiscard]]
    std::uint16_t localPort() const;

private:
    explicit ServerConnection(network::UdpSocket socket) noexcept;

    void send(const network::protocol::Packet& packet, Clock::time_point now);
    void sendRequest(Clock::time_point now);
    /// DISCONNECT with `reason`, 3 times (connected only).
    void sendDisconnect(network::protocol::DisconnectReason reason, Clock::time_point now);
    void fail(Failure::Kind kind);
    /// Handles one decoded packet coming from the server's address.
    void handle(const network::protocol::Packet& packet, Clock::time_point now,
        std::vector<network::protocol::Payload>& payloads);

    network::UdpSocket _socket;
    network::Endpoint _server;
    std::string _name;
    network::ReliableChannel _channel;
    Status _status{Status::Idle};
    int _attempts{0};
    std::optional<network::protocol::ConnectAccept> _accepted;
    std::uint32_t _token{0};
    Failure _failure;
    Clock::time_point _lastSent;
    Clock::time_point _lastReceived;
};

/// Short English sentence for the connection screen.
[[nodiscard]]
std::string describe(const ServerConnection::Failure& failure);

} // namespace client
