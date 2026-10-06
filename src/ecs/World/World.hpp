#pragma once

#include "ecs/Component/Component.hpp"
#include "ecs/Component/ComponentManager.hpp"
#include "ecs/Component/ComponentStorage.hpp"
#include "ecs/Entity/Entity.hpp"
#include "ecs/Entity/EntityManager.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace ecs {

/// The main entry point of the ECS.
///
/// The World is a facade: it hides the `EntityManager` and the
/// `ComponentManager` behind one consistent API and keeps them in sync.
/// For example, destroying an entity also removes all of its components.
///
///     ecs::World world;
///
///     ecs::Entity ship = world.createEntity();
///     world.addComponent(ship, Position{0.0F, 0.0F});
///     world.addComponent(ship, Velocity{1.0F, 0.0F});
///
///     world.query<Position, Velocity>([](ecs::Entity, Position& position, Velocity& velocity) {
///         position.x += velocity.dx;
///     });
class World {
public:
    // --- Entities -------------------------------------------------------------

    /// Creates a new entity with no component.
    [[nodiscard]]
    Entity createEntity();

    /// Destroys an entity and removes all of its components.
    /// Returns false if the entity was not alive.
    bool destroyEntity(Entity entity);

    [[nodiscard]]
    bool isAlive(Entity entity) const noexcept;

    /// Number of entities currently alive.
    [[nodiscard]]
    std::size_t entityCount() const noexcept;

    /// Destroys every entity and every component.
    void clear();

    // --- Components -----------------------------------------------------------

    /// Attaches `component` to `entity` (replacing any existing `T`)
    /// and returns a reference to the stored component.
    /// Throws `std::invalid_argument` if the entity is not alive.
    template <Component T>
    T& addComponent(Entity entity, T component)
    {
        if (!isAlive(entity)) {
            throw std::invalid_argument("World::addComponent: entity is not alive");
        }
        return _components.add(entity, std::move(component));
    }

    /// Detaches the `T` of `entity`. Returns false if it had none.
    template <Component T>
    bool removeComponent(Entity entity)
    {
        return _components.remove<T>(entity);
    }

    template <Component T>
    [[nodiscard]]
    bool hasComponent(Entity entity) const noexcept
    {
        return _components.has<T>(entity);
    }

    /// Returns the `T` of `entity`.
    /// Throws `std::out_of_range` if it has none (or if the handle is stale).
    template <Component T>
    [[nodiscard]]
    T& getComponent(Entity entity)
    {
        return _components.get<T>(entity);
    }

    template <Component T>
    [[nodiscard]]
    const T& getComponent(Entity entity) const
    {
        return _components.get<T>(entity);
    }

    /// Returns the `T` of `entity`, or nullptr if it has none.
    template <Component T>
    [[nodiscard]]
    T* tryGetComponent(Entity entity) noexcept
    {
        return _components.tryGet<T>(entity);
    }

    template <Component T>
    [[nodiscard]]
    const T* tryGetComponent(Entity entity) const noexcept
    {
        return _components.tryGet<T>(entity);
    }

    // --- Queries --------------------------------------------------------------

    /// Calls `function(entity, Ts&...)` for every entity owning ALL the
    /// component types `Ts`.
    ///
    ///     world.query<Position, Velocity, PlayerTag>(
    ///         [](ecs::Entity entity, Position& position, Velocity& velocity, PlayerTag&) {
    ///             // only players with a position and a velocity
    ///         });
    ///
    /// The iteration order is unspecified.
    ///
    /// Do not create or destroy entities, nor add or remove components,
    /// inside the callback: it would modify the storages being iterated.
    /// Defer those changes instead (see the CommandBuffer).
    template <Component... Ts, typename Function>
        requires(sizeof...(Ts) > 0) && std::invocable<Function&, Entity, Ts&...>
    void query(Function function)
    {
        const std::tuple<ComponentStorage<Ts>*...> storages{_components.findStorage<Ts>()...};

        // A component type that was never added: no entity can match.
        if (((std::get<ComponentStorage<Ts>*>(storages) == nullptr) || ...)) {
            return;
        }

        // Iterate over the smallest storage: every match must be in it,
        // and it is the one with the fewest candidates to check.
        const std::span<const Entity> candidates
            = std::min({std::get<ComponentStorage<Ts>*>(storages)->entities()...},
                [](std::span<const Entity> lhs, std::span<const Entity> rhs) {
                    return lhs.size() < rhs.size();
                });

        for (const Entity entity : candidates) {
            const bool hasEveryComponent
                = (std::get<ComponentStorage<Ts>*>(storages)->contains(entity) && ...);

            if (hasEveryComponent) {
                function(entity, std::get<ComponentStorage<Ts>*>(storages)->get(entity)...);
            }
        }
    }

private:
    EntityManager _entities;
    ComponentManager _components;
};

} // namespace ecs
