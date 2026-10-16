// Real sockets on the loopback interface, like the transport tests.

#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "network/Protocol/PacketCodec.hpp"
#include "server/Components/ReceivedPacket.hpp"
#include "server/Systems/DiscardPacketsSystem.hpp"
#include "server/Systems/NetReceiveSystem.hpp"

#include <asio/ip/address_v4.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <thread>
#include <utility>
#include <vector>

using network::Endpoint;
using network::UdpSocket;
using namespace network::protocol;
using server::DiscardPacketsSystem;
using server::NetReceiveSystem;
using server::ReceivedPacket;

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

Endpoint loopbackOf(const UdpSocket& socket)
{
    return {asio::ip::address_v4::loopback(), socket.localEndpoint()->port()};
}

void sendPacket(UdpSocket& from, const Packet& packet, const Endpoint& to)
{
    DatagramBuffer buffer{};
    const auto size = encode(packet, buffer);
    ASSERT_TRUE(size.has_value());
    ASSERT_TRUE(from.sendTo(std::span(buffer).first(*size), to).has_value());
}

std::vector<ReceivedPacket> receivedPackets(ecs::World& world)
{
    std::vector<ReceivedPacket> packets;
    world.query<ReceivedPacket>(
        [&](ecs::Entity, ReceivedPacket& packet) { packets.push_back(packet); });
    return packets;
}

/// Loopback delivery is not instantaneous: run the system until `expected`
/// datagrams were handled (accepted or dropped), for up to a second.
void receiveUntil(NetReceiveSystem& system, ecs::World& world, std::size_t expected)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (receivedPackets(world).size() + system.droppedCount() < expected
        && std::chrono::steady_clock::now() < deadline) {
        system.update(world, ecs::Time{});
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

Packet heartbeat(std::uint16_t sequence)
{
    return Packet{
        .sessionToken = 0xCAFEBABE,
        .sequence = sequence,
        .ack = std::nullopt,
        .reliableId = 0,
        .payload = Heartbeat{},
    };
}

} // namespace

TEST(NetReceiveSystem, ValidPacketBecomesAnEntity)
{
    auto server = openOrFail();
    auto client = openOrFail();
    ecs::World world;
    NetReceiveSystem system(server);

    sendPacket(client, heartbeat(7), loopbackOf(server));
    receiveUntil(system, world, 1);

    const auto packets = receivedPackets(world);
    ASSERT_EQ(packets.size(), 1U);
    EXPECT_EQ(packets[0].packet, heartbeat(7));
    EXPECT_EQ(packets[0].from.port(), client.localEndpoint()->port());
    EXPECT_EQ(system.droppedCount(), 0U);
}

TEST(NetReceiveSystem, EveryPendingDatagramIsReadInOneFrame)
{
    auto server = openOrFail();
    auto client = openOrFail();
    ecs::World world;
    NetReceiveSystem system(server);

    for (std::uint16_t sequence = 0; sequence < 5; ++sequence) {
        sendPacket(client, heartbeat(sequence), loopbackOf(server));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Let all five arrive
    system.update(world, ecs::Time{});

    EXPECT_EQ(receivedPackets(world).size(), 5U);
}

TEST(NetReceiveSystem, MalformedDatagramIsDroppedAndCounted)
{
    auto server = openOrFail();
    auto client = openOrFail();
    ecs::World world;
    NetReceiveSystem system(server);

    const std::vector<std::byte> garbage(32, std::byte{0xAB});
    ASSERT_TRUE(client.sendTo(garbage, loopbackOf(server)).has_value());
    receiveUntil(system, world, 1);

    EXPECT_EQ(system.droppedCount(), 1U);
    EXPECT_EQ(world.entityCount(), 0U);
}

TEST(NetReceiveSystem, PacketMeantForClientsIsDropped)
{
    auto server = openOrFail();
    auto client = openOrFail();
    ecs::World world;
    NetReceiveSystem system(server);

    // A SNAPSHOT only travels from the server to the clients
    const Snapshot emptySnapshot;
    const Packet snapshot{
        .sessionToken = 1,
        .sequence = 1,
        .ack = std::nullopt,
        .reliableId = 0,
        .payload = emptySnapshot,
    };
    sendPacket(client, snapshot, loopbackOf(server));
    receiveUntil(system, world, 1);

    EXPECT_EQ(system.droppedCount(), 1U);
    EXPECT_EQ(world.entityCount(), 0U);
}

TEST(NetReceiveSystem, NothingReceivedCreatesNothing)
{
    auto server = openOrFail();
    ecs::World world;
    NetReceiveSystem system(server);

    system.update(world, ecs::Time{});

    EXPECT_EQ(world.entityCount(), 0U);
}

TEST(DiscardPacketsSystem, DestroysOnlyPacketEntities)
{
    ecs::World world;
    const ecs::Entity packet = world.createEntity();
    world.addComponent(packet, ReceivedPacket{.from = {}, .packet = heartbeat(1)});
    const ecs::Entity player = world.createEntity();
    DiscardPacketsSystem system;

    system.update(world, ecs::Time{});

    EXPECT_FALSE(world.isAlive(packet));
    EXPECT_TRUE(world.isAlive(player));
}
