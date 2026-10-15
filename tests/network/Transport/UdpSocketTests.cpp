// Real sockets on the loopback interface: these tests need no network access.

#include "network/Protocol/PacketCodec.hpp"
#include "network/Transport/UdpSocket.hpp"

#include <gtest/gtest.h>

#include <asio/ip/address_v4.hpp>

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <initializer_list>
#include <optional>
#include <span>
#include <thread>
#include <utility>
#include <vector>

using network::Datagram;
using network::Endpoint;
using network::UdpSocket;
using namespace network::protocol;

namespace {

UdpSocket openOrFail()
{
    auto socket = UdpSocket::open(0);
    if (!socket) {
        ADD_FAILURE() << "cannot open socket: " << socket.error().message();
        std::abort();
    }
    return std::move(*socket);
}

/// Where to send to reach `socket` from this machine.
Endpoint loopbackOf(const UdpSocket& socket)
{
    return {asio::ip::address_v4::loopback(), socket.localEndpoint()->port()};
}

/// Loopback delivery is fast but not instantaneous: poll for up to a second.
std::optional<Datagram> waitForDatagram(UdpSocket& socket, std::span<std::byte> buffer)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < deadline) {
        auto received = socket.receive(buffer);
        EXPECT_TRUE(received.has_value()) << received.error().message();
        if (!received || *received) {
            return received ? *received : std::nullopt;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return std::nullopt;
}

std::vector<std::byte> bytes(std::initializer_list<int> values)
{
    std::vector<std::byte> result;
    for (const int value : values) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

std::vector<std::byte> contentOf(std::span<const std::byte> buffer, const Datagram& datagram)
{
    const auto content = buffer.first(datagram.size);
    return {content.begin(), content.end()};
}

} // namespace

TEST(UdpSocket, PortZeroBindsAFreePort)
{
    auto socket = openOrFail();

    const auto local = socket.localEndpoint();
    ASSERT_TRUE(local.has_value());
    EXPECT_NE(local->port(), 0);
}

TEST(UdpSocket, ReceiveReturnsNothingWhenNoDatagramIsWaiting)
{
    auto socket = openOrFail();
    DatagramBuffer buffer{};

    const auto received = socket.receive(buffer);
    ASSERT_TRUE(received.has_value());
    EXPECT_FALSE(received->has_value());
}

TEST(UdpSocket, DatagramsGoBothWays)
{
    auto server = openOrFail();
    auto client = openOrFail();
    DatagramBuffer buffer{};

    const auto request = bytes({1, 2, 3});
    ASSERT_TRUE(client.sendTo(request, loopbackOf(server)).has_value());
    const auto atServer = waitForDatagram(server, buffer);
    ASSERT_TRUE(atServer.has_value());
    EXPECT_EQ(contentOf(buffer, *atServer), request);
    EXPECT_EQ(atServer->from.port(), client.localEndpoint()->port());

    // The server answers whoever sent the request
    const auto reply = bytes({4, 5});
    ASSERT_TRUE(server.sendTo(reply, atServer->from).has_value());
    const auto atClient = waitForDatagram(client, buffer);
    ASSERT_TRUE(atClient.has_value());
    EXPECT_EQ(contentOf(buffer, *atClient), reply);
}

TEST(UdpSocket, EncodedPacketSurvivesTheTrip)
{
    auto server = openOrFail();
    auto client = openOrFail();
    const Packet packet{.sessionToken = 0xCAFEBABE, .sequence = 7, .ack = std::nullopt, .reliableId = 0, .payload = Heartbeat{}};

    DatagramBuffer out{};
    const auto size = encode(packet, out);
    ASSERT_TRUE(size.has_value());
    ASSERT_TRUE(client.sendTo(std::span(out).first(*size), loopbackOf(server)).has_value());

    DatagramBuffer in{};
    const auto received = waitForDatagram(server, in);
    ASSERT_TRUE(received.has_value());
    const auto decoded = decode(std::span(in).first(received->size), Direction::ClientToServer);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, packet);
}

TEST(UdpSocket, BindingAPortAlreadyInUseFails)
{
    auto first = openOrFail();

    const auto second = UdpSocket::open(first.localEndpoint()->port());
    EXPECT_FALSE(second.has_value());
}

TEST(UdpSocket, SendingMoreThanUdpAllowsFails)
{
    auto socket = openOrFail();
    const std::vector<std::byte> tooLarge(70000); // UDP payloads stop at 65507 bytes

    EXPECT_FALSE(socket.sendTo(tooLarge, loopbackOf(socket)).has_value());
}
