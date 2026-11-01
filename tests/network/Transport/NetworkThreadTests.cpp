// Real sockets on the loopback interface. The NetworkThread sends and receives
// on its own thread: the tests wait (up to a second) for its work to show.

#include "network/Transport/NetworkThread.hpp"
#include "network/Transport/UdpSocket.hpp"

#include <gtest/gtest.h>

#include <asio/ip/address_v4.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <utility>
#include <vector>

using network::Endpoint;
using network::NetworkThread;
using network::ReceivedDatagram;
using network::UdpSocket;
using namespace std::chrono_literals;

namespace {

std::unique_ptr<NetworkThread> openThreadOrFail()
{
    auto network = NetworkThread::open(0);
    if (!network) {
        ADD_FAILURE() << "cannot open network thread: " << network.error().message();
        std::abort();
    }
    return std::move(*network);
}

UdpSocket openSocketOrFail()
{
    auto socket = UdpSocket::open(0);
    if (!socket) {
        ADD_FAILURE() << "cannot open socket: " << socket.error().message();
        std::abort();
    }
    return std::move(*socket);
}

Endpoint loopback(std::uint16_t port)
{
    return {asio::ip::address_v4::loopback(), port};
}

/// Polls `done` for up to a second.
bool eventually(const std::function<bool()>& done)
{
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (!done()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

/// Every datagram `socket` receives within a second, until `count` arrived.
std::vector<std::vector<std::byte>> receiveOnSocket(UdpSocket& socket, std::size_t count)
{
    std::vector<std::vector<std::byte>> received;
    std::array<std::byte, 2048> buffer{};
    (void)eventually([&] {
        while (true) {
            const auto datagram = socket.receive(buffer);
            if (!datagram || !*datagram) {
                break;
            }
            const auto bytes = std::span(buffer).first((*datagram)->size);
            received.emplace_back(bytes.begin(), bytes.end());
        }
        return received.size() >= count;
    });
    return received;
}

constexpr std::array Hello{std::byte{'h'}, std::byte{'i'}};

} // namespace

TEST(NetworkThread, OpensOnAFreePort)
{
    const auto network = openThreadOrFail();

    EXPECT_NE(network->localEndpoint().port(), 0);
}

TEST(NetworkThread, PortInUseIsAnError)
{
    const auto first = openThreadOrFail();

    const auto second = NetworkThread::open(first->localEndpoint().port());

    EXPECT_FALSE(second.has_value());
}

TEST(NetworkThread, ReceivedDatagramsWaitForTheGameThread)
{
    const auto network = openThreadOrFail();
    auto client = openSocketOrFail();

    ASSERT_TRUE(client.sendTo(Hello, loopback(network->localEndpoint().port())).has_value());
    std::vector<ReceivedDatagram> received;
    ASSERT_TRUE(eventually([&] {
        auto taken = network->takeReceived();
        received.insert(received.end(), taken.begin(), taken.end());
        return !received.empty();
    }));

    ASSERT_EQ(received.size(), 1U);
    EXPECT_TRUE(std::ranges::equal(received[0].bytes, Hello));
    EXPECT_EQ(received[0].from.port(), client.localEndpoint()->port());
    EXPECT_TRUE(network->takeReceived().empty()); // Taken once only
}

TEST(NetworkThread, SendReachesThePeer)
{
    const auto network = openThreadOrFail();
    auto client = openSocketOrFail();

    network->send(Hello, loopback(client.localEndpoint()->port()));

    const auto received = receiveOnSocket(client, 1);
    ASSERT_EQ(received.size(), 1U);
    EXPECT_TRUE(std::ranges::equal(received[0], Hello));
}

TEST(NetworkThread, QueuedSendsLeaveBeforeTheThreadStops)
{
    // The server's last DISCONNECTs are queued right before it stops
    auto network = openThreadOrFail();
    auto client = openSocketOrFail();
    constexpr std::size_t count = 50;

    for (std::size_t i = 0; i < count; ++i) {
        network->send(std::vector{static_cast<std::byte>(i)}, loopback(client.localEndpoint()->port()));
    }
    network.reset();

    const auto received = receiveOnSocket(client, count);
    ASSERT_EQ(received.size(), count);
    for (std::size_t i = 0; i < count; ++i) {
        EXPECT_EQ(received[i], std::vector{static_cast<std::byte>(i)}); // In order
    }
}

TEST(NetworkThread, FloodBeyondTheQueueIsDroppedAndCounted)
{
    const auto network = openThreadOrFail();
    auto flooder = openSocketOrFail();
    constexpr std::size_t extra = 100;

    for (std::size_t i = 0; i < NetworkThread::MaxQueuedDatagrams + extra; ++i) {
        ASSERT_TRUE(flooder.sendTo(Hello, loopback(network->localEndpoint().port())).has_value());
        if (i % 32 == 31) {
            // In bursts: this tests our queue, not the OS buffer, which drops
            // datagrams itself when a slow (sanitized) build cannot keep up
            std::this_thread::sleep_for(1ms);
        }
    }
    ASSERT_TRUE(eventually([&] { return network->droppedCount() == extra; }));

    EXPECT_EQ(network->takeReceived().size(), NetworkThread::MaxQueuedDatagrams);
}

TEST(NetworkThread, StopsAtOnceWhenIdle)
{
    auto network = openThreadOrFail();
    const auto start = std::chrono::steady_clock::now();

    network.reset(); // A receive is pending: closing the socket must end it

    EXPECT_LT(std::chrono::steady_clock::now() - start, 500ms);
}
