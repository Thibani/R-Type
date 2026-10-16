#pragma once

#include "network/Transport/UdpSocket.hpp"

#include <asio/io_context.hpp>
#include <asio/ip/udp.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <span>
#include <system_error>
#include <thread>
#include <vector>

/// A UDP socket served by its own thread, so the game thread never waits on
/// the network: the subject's "the server MUST be multithreaded, so that it
/// does not block or wait for clients messages".
///
/// Like `UdpSocket`, it moves raw bytes and knows nothing about packets or the
/// ECS. Each side has its own data, so the game state needs no lock:
///
///   - the network thread alone touches the socket (Asio sockets are not safe
///     to share between threads); it receives into the inbox and sends;
///   - the game thread calls `takeReceived()` once per frame and `send()`
///     whenever it has a datagram;
///   - between them: the inbox, behind a mutex, and Asio's queue of posted
///     sends, already thread-safe.
///
///     auto network = NetworkThread::open(4242);
///     for (auto& datagram : (*network)->takeReceived()) { ... decode(datagram.bytes) ... }
///     (*network)->send(bytes, to);
///     network->reset(); // sends what is queued, then stops and joins the thread
namespace network {

struct ReceivedDatagram {
    Endpoint from;
    std::vector<std::byte> bytes;
};

class NetworkThread {
public:
    /// Datagrams waiting for the game thread at most. Beyond, new ones are
    /// dropped and counted: a flood cannot fill the memory, nor push out the
    /// datagrams of the players already queued.
    static constexpr std::size_t MaxQueuedDatagrams = 1024;

    /// Binds an IPv4 socket to `port` (0 = any free port) and starts the thread.
    /// Heap-allocated: the thread keeps a pointer to it, so it must not move.
    [[nodiscard]]
    static std::expected<std::unique_ptr<NetworkThread>, std::error_code> open(std::uint16_t port);

    /// Sends every datagram already queued, then stops and joins the thread.
    ~NetworkThread();

    NetworkThread(const NetworkThread&) = delete;
    NetworkThread& operator=(const NetworkThread&) = delete;
    NetworkThread(NetworkThread&&) = delete;
    NetworkThread& operator=(NetworkThread&&) = delete;

    /// Every datagram received since the last call, oldest first.
    [[nodiscard]]
    std::vector<ReceivedDatagram> takeReceived();

    /// Queues one datagram (copied) for the network thread, which sends them
    /// in order. A send the OS refuses is reported on stderr and lost, as UDP
    /// allows.
    void send(std::span<const std::byte> datagram, const Endpoint& to);

    /// Address the socket is bound to (gives the port chosen when opened with 0).
    [[nodiscard]]
    const Endpoint& localEndpoint() const noexcept;

    /// Datagrams dropped because `MaxQueuedDatagrams` were already waiting.
    [[nodiscard]]
    std::size_t droppedCount();

private:
    NetworkThread();

    /// Network thread: waits for the next datagram, then stores it.
    void receiveNext();
    void store(std::size_t size);

    asio::io_context _context;
    asio::ip::udp::socket _socket{_context};
    Endpoint _localEndpoint; // Read once at opening: the game thread never touches the socket

    // Network thread only. The largest UDP datagram fits, so none is truncated
    // and `decode()` sees its real size.
    std::array<std::byte, 65536> _buffer{};
    Endpoint _sender;

    std::mutex _mutex; // Guards the two members below
    std::vector<ReceivedDatagram> _inbox;
    std::size_t _dropped{0};

    std::jthread _thread; // Last: started once everything above exists
};

} // namespace network
