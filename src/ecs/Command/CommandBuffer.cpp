#include "ecs/Command/CommandBuffer.hpp"

#include "ecs/World/World.hpp"

namespace ecs {

// --- EntityBuilder ----------------------------------------------------------

EntityBuilder::EntityBuilder(std::shared_ptr<std::vector<SpawnStep>> steps) noexcept
    : _steps(std::move(steps))
{
}

EntityBuilder& EntityBuilder::then(SpawnStep step)
{
    _steps->push_back(std::move(step));
    return *this;
}

// --- CommandBuffer ----------------------------------------------------------

EntityBuilder CommandBuffer::createEntity()
{
    auto steps = std::make_shared<std::vector<SpawnStep>>();

    _commands.emplace_back([steps](World& world) {
        const Entity entity = world.createEntity();
        for (SpawnStep& step : *steps) {
            step(world, entity);
        }
    });

    return EntityBuilder{std::move(steps)};
}

void CommandBuffer::destroyEntity(Entity entity)
{
    _commands.emplace_back([entity](World& world) { world.destroyEntity(entity); });
}

void CommandBuffer::run(Command command)
{
    _commands.push_back(std::move(command));
}

void CommandBuffer::apply(World& world)
{
    if (_applying) {
        return;
    }
    _applying = true;

    // Index loop: commands may record new commands, which grows the vector.
    // Each command is moved out before running, so a reallocation is harmless.
    try {
        // NOLINTNEXTLINE(modernize-loop-convert): a range-for breaks when the vector grows
        for (std::size_t index = 0; index < _commands.size(); ++index) {
            Command command = std::move(_commands[index]);
            command(world);
        }
    } catch (...) {
        _commands.clear();
        _applying = false;
        throw;
    }

    _commands.clear();
    _applying = false;
}

std::size_t CommandBuffer::size() const noexcept
{
    return _commands.size();
}

bool CommandBuffer::empty() const noexcept
{
    return _commands.empty();
}

void CommandBuffer::clear() noexcept
{
    _commands.clear();
}

} // namespace ecs
