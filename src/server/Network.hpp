#pragma once

#include "ecs/Time/Time.hpp"
#include "network/Protocol/Packets.hpp"
#include "network/Reliability/ReliableChannel.hpp"
#include "network/Transport/UdpSocket.hpp"

namespace ecs {
class World;
} // namespace ecs

/// Helpers shared by the server's network systems.
namespace server {

using Clock = network::ReliableChannel::Clock;
using TimePoint = Clock::time_point;

/// The frame's elapsed time as a time point, for `ReliableChannel`.
/// Systems never read a clock: the game loop's time is the only source, so
/// tests can move time forward by seconds instantly.
[[nodiscard]]
TimePoint toTimePoint(const ecs::Time& time) noexcept;

/// Encodes and sends one packet. Failures are reported on stderr and the
/// packet is lost, as UDP allows: the game keeps running.
void sendPacket(network::UdpSocket& socket, const network::protocol::Packet& packet,
    const network::Endpoint& to);

/// Tells every connected client that the server stops (DISCONNECT
/// SERVER_SHUTDOWN, sent 3 times as §8.3 recommends).
void notifyShutdown(ecs::World& world, network::UdpSocket& socket, TimePoint now);

} // namespace server
