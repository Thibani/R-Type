#include "network/Transport/UdpSocket.hpp"

#include <asio/buffer.hpp>
#include <asio/error.hpp>

#include <utility>

namespace network {

UdpSocket::UdpSocket(std::unique_ptr<asio::io_context> context, asio::ip::udp::socket socket) noexcept
    : _context(std::move(context))
    , _socket(std::move(socket))
{
}

bool isTransientReceiveError(const std::error_code& error) noexcept
{
    return error == asio::error::connection_reset || error == asio::error::connection_refused
        || error == asio::error::message_size;
}

std::expected<UdpSocket, std::error_code> UdpSocket::open(std::uint16_t port)
{
    auto context = std::make_unique<asio::io_context>();
    asio::ip::udp::socket socket(*context);
    std::error_code error;

    // shortcut: IPv4 only, add a dual-stack IPv6 socket if a player ever needs it
    socket.open(asio::ip::udp::v4(), error);
    if (!error) {
        socket.bind({asio::ip::udp::v4(), port}, error);
    }
    if (!error) {
        socket.non_blocking(true, error);
    }
    if (error) {
        return std::unexpected(error);
    }
    return UdpSocket(std::move(context), std::move(socket));
}

std::expected<void, std::error_code> UdpSocket::sendTo(std::span<const std::byte> datagram, const Endpoint& to)
{
    std::error_code error;
    _socket.send_to(asio::buffer(datagram.data(), datagram.size()), to, 0, error);
    if (error) {
        return std::unexpected(error);
    }
    return {};
}

std::expected<std::optional<Datagram>, std::error_code> UdpSocket::receive(std::span<std::byte> buffer)
{
    while (true) {
        Datagram datagram;
        std::error_code error;
        datagram.size = _socket.receive_from(asio::buffer(buffer.data(), buffer.size()), datagram.from, 0, error);

        if (!error) {
            return datagram;
        }
        if (error == asio::error::would_block) {
            return std::nullopt;
        }
        if (isTransientReceiveError(error)) {
            continue; // Skip to the next datagram
        }
        return std::unexpected(error);
    }
}

std::expected<Endpoint, std::error_code> UdpSocket::localEndpoint() const
{
    std::error_code error;
    auto endpoint = _socket.local_endpoint(error);
    if (error) {
        return std::unexpected(error);
    }
    return endpoint;
}

} // namespace network
