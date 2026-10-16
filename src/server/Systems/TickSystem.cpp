#include "server/Systems/TickSystem.hpp"

#include "server/Components/NetEntity.hpp"
#include "server/Singleton.hpp"

namespace server {

void TickSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    ++singleton<SimulationTick>(world).value;
}

} // namespace server
