#pragma once

#include "ecs/Time/Time.hpp"
#include "network/Protocol/Packets.hpp"
#include "network/Reliability/ReliableChannel.hpp"
#include "network/Transport/NetworkThread.hpp"

namespace ecs {
class World;
} // namespace ecs

/// Helpers shared by the server's network systems.
namespace server {

struct NetClient;

using Clock = network::ReliableChannel::Clock;
using TimePoint = Clock::time_point;

/// The frame's elapsed time as a time point, for `ReliableChannel`.
/// Systems never read a clock: the game loop's time is the only source, so
/// tests can move time forward by seconds instantly.
[[nodiscard]]
TimePoint toTimePoint(const ecs::Time& time) noexcept;

/// Encodes one packet, in clear, and queues it for the network thread, which
/// sends it. Only for what has no session key yet: CONNECT_ACCEPT, CONNECT_REJECT.
/// Failures are reported on stderr and the packet is lost, as UDP allows:
/// the game keeps running.
void sendPacket(network::NetworkThread& socket, const network::protocol::Packet& packet,
    const network::Endpoint& to);

/// Same as `sendPacket`, encrypted for `client`: every packet of a session.
void sendToClient(
    network::NetworkThread& socket, NetClient& client, const network::protocol::Packet& packet);

/// Tells every connected client that the server stops (DISCONNECT
/// SERVER_SHUTDOWN, sent 3 times as §8.3 recommends).
void notifyShutdown(ecs::World& world, network::NetworkThread& socket, TimePoint now);

} // namespace server
