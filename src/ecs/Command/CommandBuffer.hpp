#pragma once

#include "ecs/Component/Component.hpp"
#include "ecs/Entity/Entity.hpp"

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace ecs {

class World;

/// A deferred action on the World.
using Command = std::move_only_function<void(World&)>;

/// A deferred step run on a newly created entity (add a component...).
using SpawnStep = std::move_only_function<void(World&, Entity)>;

/// Describes an entity that will be created when the commands are applied.
/// Returned by `CommandBuffer::createEntity()`:
///
///     commands.createEntity()
///         .with(Position{.x = 100.0F, .y = 50.0F})
///         .with(Velocity{.dx = 300.0F, .dy = 0.0F})
///         .with(MissileTag{});
///
/// The builder is only meaningful until the commands are applied.
class EntityBuilder {
public:
    /// Attaches `component` to the entity once it is created.
    template <Component T>
    EntityBuilder& with(T component)
    {
        // `auto&`: World only needs to be complete where the command runs.
        _steps->push_back([component = std::move(component)](auto& world, Entity entity) mutable {
            world.addComponent(entity, std::move(component));
        });
        return *this;
    }

    /// Runs `step(world, entity)` once the entity is created and has its
    /// components, e.g. to emit an event about it.
    EntityBuilder& then(SpawnStep step);

private:
    friend class CommandBuffer;

    explicit EntityBuilder(std::shared_ptr<std::vector<SpawnStep>> steps) noexcept;

    // Shared with the creation command, which runs the steps in order.
    std::shared_ptr<std::vector<SpawnStep>> _steps;
};

/// Records structural changes and applies them later, at a safe point.
///
/// Creating or destroying entities, or adding and removing components,
/// modifies the storages. Doing it inside a query would break the iteration:
///
///     world.query<Health>([&](Entity entity, Health& health) {
///         if (health.current <= 0) {
///             world.destroyEntity(entity);    // WRONG: modifies what is iterated
///             world.commands().destroyEntity(entity); // OK: done after the query
///         }
///     });
///
/// Commands are applied in the order they were recorded. `World::update()`
/// applies them after the systems have run, and again after the events have
/// been delivered.
///
/// Commands target entities that may be gone when they run: a command on a
/// dead entity (destroyed meanwhile, or stale) is silently skipped.
class CommandBuffer {
public:
    CommandBuffer() = default;
    ~CommandBuffer() = default;

    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;
    CommandBuffer(CommandBuffer&&) noexcept = default;
    CommandBuffer& operator=(CommandBuffer&&) noexcept = default;

    /// Records the creation of an entity. Use the returned builder to give it
    /// components.
    EntityBuilder createEntity();

    /// Records the destruction of `entity` (and of all its components).
    void destroyEntity(Entity entity);

    /// Records the addition (or replacement) of a component.
    template <Component T>
    void addComponent(Entity entity, T component)
    {
        // `auto&`: World only needs to be complete where the command runs.
        _commands.push_back([entity, component = std::move(component)](auto& world) mutable {
            if (world.isAlive(entity)) {
                world.addComponent(entity, std::move(component));
            }
        });
    }

    /// Records the removal of a component.
    template <Component T>
    void removeComponent(Entity entity)
    {
        _commands.push_back([entity](auto& world) { world.template removeComponent<T>(entity); });
    }

    /// Records any other deferred action.
    void run(Command command);

    /// Runs every recorded command, in recording order, then forgets them.
    /// Commands recorded while applying (by `run` or `then`) are applied too.
    /// Calling it from inside a command does nothing (not re-entrant).
    void apply(World& world);

    /// Number of commands waiting to be applied.
    [[nodiscard]]
    std::size_t size() const noexcept;

    [[nodiscard]]
    bool empty() const noexcept;

    /// Forgets every recorded command without running it.
    void clear() noexcept;

private:
    std::vector<Command> _commands;
    bool _applying{false};
};

} // namespace ecs
