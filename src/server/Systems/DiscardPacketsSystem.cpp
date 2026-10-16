#include "server/Systems/DiscardPacketsSystem.hpp"

#include "ecs/World/World.hpp"
#include "server/Components/ClientMessage.hpp"
#include "server/Components/ReceivedPacket.hpp"

#include <vector>

namespace server {

namespace {

template <typename Message>
void discardAll(ecs::World& world)
{
    // Collected first: entities must not be destroyed while being queried
    std::vector<ecs::Entity> leftovers;
    world.query<Message>(
        [&](ecs::Entity entity, Message& /*message*/) { leftovers.push_back(entity); });

    for (const ecs::Entity entity : leftovers) {
        world.destroyEntity(entity);
    }
}

} // namespace

void DiscardPacketsSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    discardAll<ReceivedPacket>(world);
    discardAll<IncompatibleClient>(world);
    discardAll<ClientMessage>(world);
}

} // namespace server
