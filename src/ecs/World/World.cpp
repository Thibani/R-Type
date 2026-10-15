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
    _events.clear();
}

SystemManager& World::systems() noexcept
{
    return _systems;
}

const SystemManager& World::systems() const noexcept
{
    return _systems;
}

void World::update(const Time& time)
{
    _systems.update(*this, time);
    _events.dispatch();
}

EventBus& World::events() noexcept
{
    return _events;
}

const EventBus& World::events() const noexcept
{
    return _events;
}

} // namespace ecs
