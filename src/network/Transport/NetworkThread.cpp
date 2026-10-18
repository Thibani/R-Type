#include "network/Transport/NetworkThread.hpp"

#include <asio/buffer.hpp>
#include <asio/error.hpp>
#include <asio/post.hpp>

#include <iostream>

namespace network {

NetworkThread::NetworkThread() = default;

std::expected<std::unique_ptr<NetworkThread>, std::error_code> NetworkThread::open(std::uint16_t port)
{
    std::unique_ptr<NetworkThread> network(new NetworkThread()); // Private constructor: no make_unique
    std::error_code error;
    // shortcut: IPv4 only, like UdpSocket
    network->_socket.open(asio::ip::udp::v4(), error);
    if (!error) {
        network->_socket.bind({asio::ip::udp::v4(), port}, error);
    }
    if (!error) {
        // A full OS buffer then drops the datagram instead of stalling the thread
        network->_socket.non_blocking(true, error);
    }
    if (!error) {
        network->_localEndpoint = network->_socket.local_endpoint(error);
    }
    if (error) {
        return std::unexpected(error);
    }

    network->receiveNext();
    // run() returns once the socket is closed and the queued sends are done
    network->_thread = std::jthread([raw = network.get()] { raw->_context.run(); });
    return network;
}

NetworkThread::~NetworkThread()
{
    if (!_thread.joinable()) {
        return; // open() failed: no thread to stop
    }
    // Posted after every send already queued, so they all go out first
    asio::post(_context, [this] {
        std::error_code ignored;
        _socket.close(ignored); // Cancels the pending receive: no work left
    });
    _thread.join();
}

std::vector<ReceivedDatagram> NetworkThread::takeReceived()
{
    std::vector<ReceivedDatagram> received;
    const std::scoped_lock lock(_mutex);
    received.swap(_inbox); // The game thread holds the lock for a swap only
    return received;
}

void NetworkThread::send(std::span<const std::byte> datagram, const Endpoint& to)
{
    asio::post(_context, [this, bytes = std::vector<std::byte>(datagram.begin(), datagram.end()), to] {
        std::error_code error;
        _socket.send_to(asio::buffer(bytes), to, 0, error);
        if (error) {
            std::cerr << "network: send to " << to << " failed: " << error.message() << '\n';
        }
    });
}

const Endpoint& NetworkThread::localEndpoint() const noexcept
{
    return _localEndpoint;
}

std::size_t NetworkThread::droppedCount()
{
    const std::scoped_lock lock(_mutex);
    return _dropped;
}

void NetworkThread::receiveNext()
{
    _socket.async_receive_from(
        asio::buffer(_buffer), _sender, [this](const std::error_code& error, std::size_t size) {
            // Closed by the destructor: the thread is stopping. A datagram
            // received just before the close still ends here, without re-arming.
            if (error == asio::error::operation_aborted || !_socket.is_open()) {
                return;
            }
            if (!error) {
                store(size);
            } else if (!isTransientReceiveError(error)) {
                // Never stop receiving: the game must go on (§8.2 timeouts handle the rest)
                std::cerr << "network: receive failed: " << error.message() << '\n';
            }
            receiveNext();
        });
}

void NetworkThread::store(std::size_t size)
{
    const std::span<const std::byte> datagram = std::span(_buffer).first(size);
    const std::scoped_lock lock(_mutex);
    if (_inbox.size() >= MaxQueuedDatagrams) {
        ++_dropped;
        return;
    }
    _inbox.push_back(ReceivedDatagram{.from = _sender, .bytes = {datagram.begin(), datagram.end()}});
}

} // namespace network
