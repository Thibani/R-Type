#include "ecs/World/World.hpp"

#include <span>
#include <string>
#include <vector>

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
    _commands.clear();
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
    // Copied: a system may add a stage while the frame runs.
    const std::span<const std::string> scheduled = _systems.scheduler().stages();
    const std::vector<std::string> stages(scheduled.begin(), scheduled.end());

    for (const std::string& stage : stages) {
        _systems.updateStage(*this, time, stage);
        synchronize();
    }
}

void World::synchronize()
{
    _commands.apply(*this);
    _events.dispatch();
    _commands.apply(*this);
}

EventBus& World::events() noexcept
{
    return _events;
}

const EventBus& World::events() const noexcept
{
    return _events;
}

CommandBuffer& World::commands() noexcept
{
    return _commands;
}

} // namespace ecs
