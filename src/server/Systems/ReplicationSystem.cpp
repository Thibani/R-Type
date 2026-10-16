#include "server/Systems/ReplicationSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "network/Protocol/Packets.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/NetEntity.hpp"

#include <cstdint>
#include <map>

namespace server {

using namespace network::protocol;

void ReplicationSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    std::map<std::uint16_t, EntitySpawn> alive;
    world.query<NetEntity, game::Position, game::Velocity>([&](ecs::Entity /*entity*/,
                                                               NetEntity& net,
                                                               game::Position& position,
                                                               game::Velocity& velocity) {
        alive.emplace(net.netId,
            EntitySpawn{
                .netId = net.netId,
                .kind = net.kind,
                .owner = net.owner,
                .x = quantize(position.x),
                .y = quantize(position.y),
                .vx = quantize(velocity.dx),
                .vy = quantize(velocity.dy),
            });
    });

    world.query<NetClient>([&](ecs::Entity /*entity*/, NetClient& client) {
        for (auto known = client.knownEntities.begin(); known != client.knownEntities.end();) {
            if (alive.contains(*known)) {
                ++known;
                continue;
            }
            client.channel.queueReliable(EntityDestroy{.netId = *known});
            known = client.knownEntities.erase(known);
        }
        for (const auto& [netId, spawn] : alive) {
            if (client.knownEntities.insert(netId).second) {
                client.channel.queueReliable(spawn);
            }
        }
    });
}

} // namespace server
