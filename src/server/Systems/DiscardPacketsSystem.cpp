#include "server/Systems/DiscardPacketsSystem.hpp"

#include "ecs/World/World.hpp"
#include "server/Components/ReceivedPacket.hpp"

#include <vector>

namespace server {

void DiscardPacketsSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    // Collected first: entities must not be destroyed while being queried
    std::vector<ecs::Entity> leftovers;
    world.query<ReceivedPacket>(
        [&](ecs::Entity entity, ReceivedPacket& /*packet*/) { leftovers.push_back(entity); });

    for (const ecs::Entity entity : leftovers) {
        world.destroyEntity(entity);
    }
}

} // namespace server
