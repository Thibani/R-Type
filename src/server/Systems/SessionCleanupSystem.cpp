#include "server/Systems/SessionCleanupSystem.hpp"

#include "ecs/World/World.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Network.hpp"

#include <iostream>
#include <string_view>
#include <vector>

namespace server {

using namespace network::protocol;

namespace {

std::string_view describe(LeaveReason reason) noexcept
{
    switch (reason) {
    case LeaveReason::Quit:
        return "quit";
    case LeaveReason::Timeout:
        return "timed out";
    case LeaveReason::Kicked:
        return "was kicked";
    }
    return "left";
}

} // namespace

SessionCleanupSystem::SessionCleanupSystem(network::UdpSocket& socket) noexcept
    : _socket(socket)
{
}

void SessionCleanupSystem::update(ecs::World& world, const ecs::Time& time)
{
    const TimePoint now = toTimePoint(time);

    std::vector<ecs::Entity> leaving;
    std::vector<PlayerLeft> announcements;
    world.query<NetClient, Disconnecting>(
        [&](ecs::Entity entity, NetClient& client, Disconnecting& disconnecting) {
            if (disconnecting.notify) {
                for (int i = 0; i < 3; ++i) { // No acknowledgement: repeat instead (§8.3)
                    const Packet packet
                        = client.channel.wrap(Disconnect{.reason = *disconnecting.notify}, now);
                    sendPacket(_socket, packet, client.endpoint);
                }
            }
            std::cout << "r-type_server: player " << static_cast<int>(client.playerId) << " '"
                      << client.name << "' " << describe(disconnecting.reason) << '\n';
            leaving.push_back(entity);
            announcements.push_back(
                PlayerLeft{.playerId = client.playerId, .reason = disconnecting.reason});
        });

    for (const ecs::Entity entity : leaving) {
        world.destroyEntity(entity);
    }
    world.query<NetClient>([&](ecs::Entity /*entity*/, NetClient& client) {
        for (const PlayerLeft& left : announcements) {
            client.channel.queueReliable(left);
        }
    });
}

} // namespace server
