#pragma once

#include "ecs/Component/Component.hpp"
#include "ecs/Entity/Entity.hpp"

#include <cstddef>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ecs {

/// Type-erased view of a component storage.
///
/// It exposes only what can be done without knowing the component type,
/// which lets the `ComponentManager` keep storages of different types in a
/// single container and remove every component of a destroyed entity.
class IComponentStorage {
public:
    virtual ~IComponentStorage() = default;

    IComponentStorage(const IComponentStorage&) = delete;
    IComponentStorage& operator=(const IComponentStorage&) = delete;
    IComponentStorage(IComponentStorage&&) = delete;
    IComponentStorage& operator=(IComponentStorage&&) = delete;

    /// Removes the component of `entity`. Returns false if it had none.
    virtual bool remove(Entity entity) = 0;

    [[nodiscard]]
    virtual bool contains(Entity entity) const noexcept
        = 0;

    /// Number of components stored.
    [[nodiscard]]
    virtual std::size_t size() const noexcept
        = 0;

    virtual void clear() noexcept = 0;

protected:
    IComponentStorage() = default;
};

/// Stores every component of type `T`, using a sparse set.
///
/// Layout, for 3 entities owning a `T`:
///
///     _sparse     [ 0 | - | 2 | 1 | - ]   indexed by entity id -> index in the dense arrays
///     _entities   [ #0 | #3 | #2 ]        dense, packed without holes
///     _components [ c0 | c3 | c2 ]        dense, _components[i] belongs to _entities[i]
///
/// - lookup, insertion and removal are O(1);
/// - components are packed contiguously, so iterating over them is cache friendly;
/// - removal swaps the last element into the hole: order is not preserved.
///
/// Handles are compared with their generation, so a stale handle never
/// matches the component of the entity that reused its id.
template <Component T>
class ComponentStorage final : public IComponentStorage {
public:
    /// Attaches `component` to `entity` and returns a reference to it.
    ///
    /// Each entity id owns at most one slot. If that slot is already used,
    /// it is overwritten. This covers two cases:
    ///  - the entity already owns a `T`: the component is replaced;
    ///  - a previous owner of the recycled id left a stale `T` behind:
    ///    it is discarded and the slot is given to the new entity.
    T& insert(Entity entity, T component)
    {
        const auto id = static_cast<std::size_t>(entity.id());
        if (id >= _sparse.size()) {
            _sparse.resize(id + 1, NoIndex);
        }

        const std::size_t existingIndex = _sparse[id];
        if (existingIndex != NoIndex) {
            _entities[existingIndex] = entity;
            _components[existingIndex] = std::move(component);
            return _components[existingIndex];
        }

        _sparse[id] = _entities.size();
        _entities.push_back(entity);
        _components.push_back(std::move(component));

        return _components.back();
    }

    bool remove(Entity entity) override
    {
        if (!contains(entity)) {
            return false;
        }

        const std::size_t removedIndex = _sparse[entity.id()];
        const std::size_t lastIndex = _entities.size() - 1;

        // Fill the hole with the last element, then drop the last element.
        if (removedIndex != lastIndex) {
            _entities[removedIndex] = _entities[lastIndex];
            _components[removedIndex] = std::move(_components[lastIndex]);
            _sparse[_entities[removedIndex].id()] = removedIndex;
        }

        _entities.pop_back();
        _components.pop_back();
        _sparse[entity.id()] = NoIndex;

        return true;
    }

    [[nodiscard]]
    bool contains(Entity entity) const noexcept override
    {
        const auto id = static_cast<std::size_t>(entity.id());
        if (!entity.valid() || id >= _sparse.size() || _sparse[id] == NoIndex) {
            return false;
        }
        return _entities[_sparse[id]] == entity; // Also rejects stale generations
    }

    /// Returns the component of `entity`.
    /// Throws `std::out_of_range` if the entity has none: asking for a missing
    /// component is a programming error. Use `tryGet()` when it is optional.
    [[nodiscard]]
    T& get(Entity entity)
    {
        T* component = tryGet(entity);
        if (component == nullptr) {
            throw std::out_of_range("ComponentStorage::get: entity has no such component");
        }
        return *component;
    }

    [[nodiscard]]
    const T& get(Entity entity) const
    {
        const T* component = tryGet(entity);
        if (component == nullptr) {
            throw std::out_of_range("ComponentStorage::get: entity has no such component");
        }
        return *component;
    }

    /// Returns the component of `entity`, or nullptr if it has none.
    [[nodiscard]]
    T* tryGet(Entity entity) noexcept
    {
        if (!contains(entity)) {
            return nullptr;
        }
        return &_components[_sparse[entity.id()]];
    }

    [[nodiscard]]
    const T* tryGet(Entity entity) const noexcept
    {
        if (!contains(entity)) {
            return nullptr;
        }
        return &_components[_sparse[entity.id()]];
    }

    [[nodiscard]]
    std::size_t size() const noexcept override
    {
        return _entities.size();
    }

    /// Entities owning a `T`, packed. `entities()[i]` owns `components()[i]`.
    [[nodiscard]]
    std::span<const Entity> entities() const noexcept
    {
        return _entities;
    }

    [[nodiscard]]
    std::span<T> components() noexcept
    {
        return _components;
    }

    [[nodiscard]]
    std::span<const T> components() const noexcept
    {
        return _components;
    }

    void clear() noexcept override
    {
        _sparse.clear();
        _entities.clear();
        _components.clear();
    }

private:
    static constexpr std::size_t NoIndex = std::numeric_limits<std::size_t>::max();

    std::vector<std::size_t> _sparse; // Entity id -> index in the dense arrays
    std::vector<Entity> _entities; // Dense: owner of each component
    std::vector<T> _components; // Dense: the components themselves
};

} // namespace ecs
