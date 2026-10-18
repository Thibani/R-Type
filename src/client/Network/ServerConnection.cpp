#include "client/Network/ServerConnection.hpp"

#include "network/Protocol/PacketCodec.hpp"

#include <asio/io_context.hpp>
#include <asio/ip/udp.hpp>

#include <span>
#include <string>
#include <utility>
#include <variant>

namespace client {

using namespace network::protocol;

std::expected<ServerConnection, std::error_code> ServerConnection::open()
{
    auto socket = network::UdpSocket::open(0); // Any free port: the server answers to it
    if (!socket) {
        return std::unexpected(socket.error());
    }
    return ServerConnection(std::move(*socket));
}

ServerConnection::ServerConnection(network::UdpSocket socket) noexcept
    : _socket(std::move(socket))
{
}

void ServerConnection::connect(
    std::string_view host, std::uint16_t port, std::string name, Clock::time_point now)
{
    _channel = network::ReliableChannel(0); // A new session starts from sequence 0
    _accepted.reset();
    _token = 0;
    _attempts = 0;
    _name = std::move(name);

    // Synchronous and done once per click: a name like "localhost" also works
    asio::io_context context;
    asio::ip::udp::resolver resolver(context);
    std::error_code error;
    const auto results
        = resolver.resolve(asio::ip::udp::v4(), std::string(host), std::to_string(port), error);
    if (error || results.empty()) {
        fail(Failure::Kind::BadAddress);
        return;
    }
    _server = results.begin()->endpoint();
    _status = Status::Connecting;
    sendRequest(now);
}

std::vector<Payload> ServerConnection::receive(Clock::time_point now)
{
    std::vector<Payload> payloads;
    DatagramBuffer buffer{};
    while (true) {
        auto received = _socket.receive(buffer);
        if (!received || !*received) {
            break; // Socket error or nothing left this frame
        }
        if (_status != Status::Connecting && _status != Status::Connected) {
            continue; // No session: drain and ignore
        }
        if ((*received)->from != _server) {
            continue; // Only the server we talk to may speak
        }
        const auto packet
            = decode(std::span(buffer).first((*received)->size), Direction::ServerToClient);
        if (!packet) {
            continue; // §7: a malformed datagram changes nothing
        }
        handle(*packet, now, payloads);
    }
    return payloads;
}

void ServerConnection::handle(
    const Packet& packet, Clock::time_point now, std::vector<Payload>& payloads)
{
    if (const auto* reject = std::get_if<ConnectReject>(&packet.payload)) {
        if (_status == Status::Connecting) {
            _failure.reject = reject->reason;
            fail(Failure::Kind::Rejected);
        }
        return;
    }

    if (_status == Status::Connecting) {
        const auto* accept = std::get_if<ConnectAccept>(&packet.payload);
        if (accept == nullptr || packet.sessionToken == 0) {
            return; // Anything else waits: the server resends it until acknowledged
        }
        _token = packet.sessionToken;
        _channel.setSessionToken(_token);
        _accepted = *accept;
        _status = Status::Connected;
        _lastReceived = now;
        // Only so that our next packets acknowledge it; the payload was handled here
        [[maybe_unused]] const auto delivered = _channel.receive(packet, now);
        return;
    }

    if (packet.sessionToken != _token) {
        return; // §7 rule 10: not our session
    }
    _lastReceived = now;
    auto delivered = _channel.receive(packet, now);
    if (!delivered) {
        // The server sent too many reliable messages out of order
        sendDisconnect(DisconnectReason::ProtocolError, now);
        fail(Failure::Kind::ProtocolError);
        return;
    }
    for (Payload& payload : *delivered) {
        if (const auto* disconnect = std::get_if<Disconnect>(&payload)) {
            _failure.disconnect = disconnect->reason;
            fail(Failure::Kind::Disconnected);
            return;
        }
        if (std::holds_alternative<Heartbeat>(payload)
            || std::holds_alternative<ConnectAccept>(payload)) {
            continue; // Keep-alive, or a repeated ACCEPT: nothing for the game
        }
        payloads.push_back(std::move(payload));
    }
}

void ServerConnection::update(Clock::time_point now)
{
    if (_status == Status::Connecting && now - _lastSent >= RetryDelay) {
        if (_attempts >= MaxAttempts) {
            fail(Failure::Kind::NoAnswer);
        } else {
            sendRequest(now);
        }
    } else if (_status == Status::Connected) {
        if (now - _lastReceived >= Timeout) {
            fail(Failure::Kind::ConnectionLost);
        } else if (now - _lastSent >= HeartbeatDelay) {
            send(_channel.wrap(Heartbeat{}, now), now);
        }
    }
}

void ServerConnection::sendInput(const Input& input, Clock::time_point now)
{
    if (_status == Status::Connected) {
        send(_channel.wrap(input, now), now);
    }
}

void ServerConnection::disconnect(Clock::time_point now)
{
    sendDisconnect(DisconnectReason::Quit, now);
    _status = Status::Idle;
    _accepted.reset();
}

void ServerConnection::sendDisconnect(DisconnectReason reason, Clock::time_point now)
{
    if (_status != Status::Connected) {
        return;
    }
    for (int i = 0; i < DisconnectRepeats; ++i) {
        send(_channel.wrap(Disconnect{.reason = reason}, now), now);
    }
}

ServerConnection::Status ServerConnection::status() const noexcept
{
    return _status;
}

int ServerConnection::attempts() const noexcept
{
    return _attempts;
}

const std::optional<ConnectAccept>& ServerConnection::accepted() const noexcept
{
    return _accepted;
}

const ServerConnection::Failure& ServerConnection::failure() const noexcept
{
    return _failure;
}

std::uint16_t ServerConnection::localPort() const
{
    const auto endpoint = _socket.localEndpoint();
    return endpoint ? endpoint->port() : 0;
}

void ServerConnection::send(const Packet& packet, Clock::time_point now)
{
    DatagramBuffer buffer{};
    const auto size = encode(packet, buffer);
    if (!size) {
        return; // Our own payload is invalid: nothing sensible to send
    }
    if (!_socket.sendTo(std::span(buffer).first(*size), _server)) {
        return; // Lost, as UDP allows: the next packet or retry makes up for it
    }
    _lastSent = now;
}

void ServerConnection::sendRequest(Clock::time_point now)
{
    send(_channel.wrap(ConnectRequest{.name = _name}, now), now);
    _lastSent = now; // Even if the OS refused it: retry in 500 ms, not at once
    ++_attempts;
}

void ServerConnection::fail(Failure::Kind kind)
{
    _status = Status::Failed;
    _failure.kind = kind;
    _accepted.reset();
}

std::string describe(const ServerConnection::Failure& failure)
{
    using Kind = ServerConnection::Failure::Kind;
    switch (failure.kind) {
    case Kind::BadAddress:
        return "Unknown server address.";
    case Kind::NoAnswer:
        return "No answer from the server.";
    case Kind::ConnectionLost:
        return "Connection lost: no news from the server for 5 seconds.";
    case Kind::ProtocolError:
        return "The server sent invalid data.";
    case Kind::Rejected:
        switch (failure.reject) {
        case RejectReason::ServerFull:
            return "Server full. Try again later.";
        case RejectReason::VersionMismatch:
            return "The server uses another protocol version.";
        case RejectReason::GameInProgress:
            return "A game is already running. Try again later.";
        case RejectReason::InvalidName:
            return "The server refused this name.";
        }
        break;
    case Kind::Disconnected:
        switch (failure.disconnect) {
        case DisconnectReason::Quit:
            return "The server closed the session.";
        case DisconnectReason::Timeout:
            return "The server lost contact with you.";
        case DisconnectReason::Kicked:
            return "You were kicked by the server.";
        case DisconnectReason::ServerShutdown:
            return "The server shut down.";
        case DisconnectReason::ProtocolError:
            return "Disconnected: protocol error.";
        }
        break;
    }
    return "Connection failed.";
}

} // namespace client
