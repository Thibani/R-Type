#include "ecs/World/World.hpp"

namespace ecs {

Entity World::createEntity()
{
    return _entities.create();
}

bool World::destroyEntity(Entity entity)
{
    if (!_entities.destroy(entity)) {
        return false;
    }
    _components.removeAll(entity);
    return true;
}

bool World::isAlive(Entity entity) const noexcept
{
    return _entities.isAlive(entity);
}

std::size_t World::entityCount() const noexcept
{
    return _entities.aliveCount();
}

void World::clear()
{
    _components.clear();
    _entities.clear();
}

} // namespace ecs
