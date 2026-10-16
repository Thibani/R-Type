#include "server/Systems/NetSendSystem.hpp"

#include "ecs/World/World.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Network.hpp"

namespace server {

using namespace network::protocol;

NetSendSystem::NetSendSystem(network::UdpSocket& socket) noexcept
    : _socket(socket)
{
}

void NetSendSystem::update(ecs::World& world, const ecs::Time& time)
{
    const TimePoint now = toTimePoint(time);

    world.query<NetClient>([&](ecs::Entity /*entity*/, NetClient& client) {
        const auto due = client.channel.takeDue(now);
        for (const Packet& packet : due) {
            sendPacket(_socket, packet, client.endpoint);
        }
        if (!due.empty()) {
            client.lastSent = now;
        } else if (now - client.lastSent >= HeartbeatInterval) {
            sendPacket(_socket, client.channel.wrap(Heartbeat{}, now), client.endpoint);
            client.lastSent = now;
        }
    });
}

} // namespace server
