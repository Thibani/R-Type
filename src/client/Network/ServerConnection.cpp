#include "client/Network/ServerConnection.hpp"

#include "network/Crypto/SealedDatagram.hpp"
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
    std::string_view host, std::uint16_t port, Credentials credentials, Clock::time_point now)
{
    _channel = network::ReliableChannel(0); // A new session starts from sequence 0
    _accepted.reset();
    _token = 0;
    _cipher.reset();
    _keys = network::crypto::generateKeyPair(); // A new key for each session
    _attempts = 0;
    _credentials = std::move(credentials);

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
        if (_status == Status::Idle || _status == Status::Failed) {
            continue; // No session: drain and ignore
        }
        if ((*received)->from != _server) {
            continue; // Only the server we talk to may speak
        }
        std::span<const std::byte> datagram = std::span(buffer).first((*received)->size);
        DatagramBuffer opened{};
        if (_cipher) { // Connected: whatever is not sealed with our key is not the server
            const auto size = network::crypto::openDatagram(*_cipher, datagram, opened);
            if (!size) {
                continue;
            }
            datagram = std::span(opened).first(*size);
        }
        const auto packet = decode(datagram, Direction::ServerToClient);
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
        const auto sessionKey = network::crypto::deriveSessionKey(
            _keys, accept->publicKey, network::crypto::Role::Client);
        if (!sessionKey) {
            fail(Failure::Kind::ProtocolError);
            return;
        }
        _cipher.emplace(*sessionKey, network::crypto::Role::Client);
        _keys = {}; // The secret key has done its job
        _token = packet.sessionToken;
        _channel.setSessionToken(_token);
        _accepted = *accept;
        _status = Status::LoggingIn;
        _lastReceived = now;
        // Only so that our next packets acknowledge it; the payload was handled here
        [[maybe_unused]] const auto delivered = _channel.receive(packet, now);
        _attempts = 0;
        sendLogin(now);
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
        if (std::holds_alternative<Heartbeat>(payload)) {
            continue; // Keep-alive: nothing for the game
        }
        if (const auto* result = std::get_if<AuthResult>(&payload)) {
            if (_status != Status::LoggingIn) {
                continue;
            }
            _credentials.password.clear();
            if (result->code != AuthCode::Ok) {
                sendDisconnect(DisconnectReason::Quit, now); // Frees the player slot at once
                _failure.auth = result->code;
                fail(Failure::Kind::AuthFailed);
                return;
            }
            _status = Status::Connected;
            continue;
        }
        if (_status == Status::Connected) {
            // Before AUTH_RESULT only a snapshot can arrive, not meant for us yet:
            // a reliable event cannot, since AUTH_RESULT is the first of them
            payloads.push_back(std::move(payload));
        }
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
    } else if (_status == Status::LoggingIn) {
        if (now - _lastReceived >= Timeout) {
            fail(Failure::Kind::ConnectionLost);
        } else if (now - _lastSent >= RetryDelay) {
            if (_attempts >= MaxLoginAttempts) {
                fail(Failure::Kind::NoAnswer);
            } else {
                sendLogin(now);
            }
        }
    } else if (_status == Status::Connected) {
        if (now - _lastReceived >= Timeout) {
            fail(Failure::Kind::ConnectionLost);
            return;
        }
        for (const Packet& packet : _channel.takeDue(now)) {
            send(packet, now);
        }
        if (now - _lastSent >= HeartbeatDelay) {
            send(_channel.wrap(Heartbeat{}, now), now);
        }
    }
}

void ServerConnection::sendReliable(Payload payload)
{
    if (_status == Status::Connected) {
        _channel.queueReliable(std::move(payload));
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
    _credentials.password.clear();
    _accepted.reset();
    _cipher.reset();
}

void ServerConnection::sendDisconnect(DisconnectReason reason, Clock::time_point now)
{
    if (_status != Status::Connected && _status != Status::LoggingIn) {
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
    std::span<const std::byte> datagram = std::span(buffer).first(*size);
    DatagramBuffer sealed{};
    if (_cipher) {
        const auto sealedSize = network::crypto::sealDatagram(*_cipher, datagram, sealed);
        if (!sealedSize) {
            return; // Our own packet is too large: nothing sensible to send
        }
        datagram = std::span(sealed).first(*sealedSize);
    }
    if (!_socket.sendTo(datagram, _server)) {
        return; // Lost, as UDP allows: the next packet or retry makes up for it
    }
    _lastSent = now;
}

void ServerConnection::sendRequest(Clock::time_point now)
{
    send(
        _channel.wrap(ConnectRequest{.name = _credentials.name, .publicKey = _keys.publicKey}, now),
        now);
    _lastSent = now; // Even if the OS refused it: retry in 500 ms, not at once
    ++_attempts;
}

void ServerConnection::sendLogin(Clock::time_point now)
{
    send(_channel.wrap(
             AuthRequest{.mode = _credentials.mode, .password = _credentials.password}, now),
        now);
    _lastSent = now;
    ++_attempts;
}

void ServerConnection::fail(Failure::Kind kind)
{
    _status = Status::Failed;
    _failure.kind = kind;
    _credentials.password.clear();
    _accepted.reset();
    _cipher.reset();
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
    case Kind::AuthFailed:
        switch (failure.auth) {
        case AuthCode::Ok: // Not a failure: never reported
        case AuthCode::WrongCredentials:
            return "Wrong name or password.";
        case AuthCode::NameTaken:
            return "This name is already taken.";
        case AuthCode::AlreadyConnected:
            return "This account is already in the game.";
        case AuthCode::InvalidPassword:
            return "The password must have 8 to 64 characters.";
        case AuthCode::InvalidName:
            return "The server refused this name.";
        case AuthCode::ServerError:
            return "The server could not handle your account.";
        }
        break;
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
