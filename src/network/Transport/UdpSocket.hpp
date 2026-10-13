#pragma once

#include <asio/io_context.hpp>
#include <asio/ip/udp.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <system_error>

/// Non-blocking UDP socket: the only code that talks to the operating system.
///
/// It moves raw bytes and knows nothing about packets or the ECS. Every call
/// returns immediately, so the game loop can drain it once per tick:
///
///     auto socket = UdpSocket::open(4242); // server; 0 = any free port (client)
///     DatagramBuffer buffer{};
///     while (true) {
///         auto received = socket->receive(buffer);
///         if (!received || !*received) {
///             break; // socket error (log received.error()) or nothing left this tick
///         }
///         auto bytes = std::span(buffer).first((*received)->size);
///         // decode(bytes, ...) and handle the sender (*received)->from
///     }
///
/// Errors are returned as `std::error_code`, never thrown.
namespace network {

/// Address and port of the other side. Hashable, so it can key a map of clients.
using Endpoint = asio::ip::udp::endpoint;

struct Datagram {
    Endpoint from;
    std::size_t size{0}; // Bytes written at the start of the receive buffer
};

class UdpSocket {
public:
    /// Binds an IPv4 socket to `port` on every interface (0 = let the OS choose).
    [[nodiscard]]
    static std::expected<UdpSocket, std::error_code> open(std::uint16_t port);

    /// Sends one datagram. Fails if the OS refuses it (e.g. too large,
    /// network unreachable); the datagram is then lost, as UDP allows.
    std::expected<void, std::error_code> sendTo(std::span<const std::byte> datagram, const Endpoint& to);

    /// Reads one pending datagram into `buffer`, or returns `std::nullopt`
    /// if none is waiting. A datagram longer than `buffer` is truncated
    /// (Linux) or skipped (Windows); pass a `DatagramBuffer` and let `decode()` reject it.
    [[nodiscard]]
    std::expected<std::optional<Datagram>, std::error_code> receive(std::span<std::byte> buffer);

    /// Address the socket is bound to (gives the port chosen when opened with 0).
    [[nodiscard]]
    std::expected<Endpoint, std::error_code> localEndpoint() const;

private:
    UdpSocket(std::unique_ptr<asio::io_context> context, asio::ip::udp::socket socket) noexcept;

    std::unique_ptr<asio::io_context> _context; // Heap-allocated: the socket keeps a reference, and UdpSocket must stay movable
    asio::ip::udp::socket _socket;
};

} // namespace network
