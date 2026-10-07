#pragma once

#include "ecs/Component/Component.hpp"
#include "ecs/Component/ComponentStorage.hpp"
#include "ecs/Entity/Entity.hpp"

#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace ecs {

/// Owns one `ComponentStorage` per component type.
///
/// Storages are created on demand, the first time a component type is added:
/// adding a new component type never requires modifying the ECS.
///
/// The manager does not check whether entities are alive; the `World`
/// is responsible for that. It only maps (entity, type) -> component.
class ComponentManager {
public:
    /// Attaches `component` to `entity` (replacing any existing `T`).
    template <Component T>
    T& add(Entity entity, T component)
    {
        return storage<T>().insert(entity, std::move(component));
    }

    /// Detaches the `T` of `entity`. Returns false if it had none.
    template <Component T>
    bool remove(Entity entity)
    {
        ComponentStorage<T>* typedStorage = findStorage<T>();
        return typedStorage != nullptr && typedStorage->remove(entity);
    }

    template <Component T>
    [[nodiscard]]
    bool has(Entity entity) const noexcept
    {
        const ComponentStorage<T>* typedStorage = findStorage<T>();
        return typedStorage != nullptr && typedStorage->contains(entity);
    }

    /// Returns the `T` of `entity`. Throws `std::out_of_range` if it has none.
    template <Component T>
    [[nodiscard]]
    T& get(Entity entity)
    {
        ComponentStorage<T>* typedStorage = findStorage<T>();
        if (typedStorage == nullptr) {
            throw std::out_of_range("ComponentManager::get: entity has no such component");
        }
        return typedStorage->get(entity);
    }

    template <Component T>
    [[nodiscard]]
    const T& get(Entity entity) const
    {
        const ComponentStorage<T>* typedStorage = findStorage<T>();
        if (typedStorage == nullptr) {
            throw std::out_of_range("ComponentManager::get: entity has no such component");
        }
        return typedStorage->get(entity);
    }

    /// Returns the `T` of `entity`, or nullptr if it has none.
    template <Component T>
    [[nodiscard]]
    T* tryGet(Entity entity) noexcept
    {
        ComponentStorage<T>* typedStorage = findStorage<T>();
        return typedStorage != nullptr ? typedStorage->tryGet(entity) : nullptr;
    }

    template <Component T>
    [[nodiscard]]
    const T* tryGet(Entity entity) const noexcept
    {
        const ComponentStorage<T>* typedStorage = findStorage<T>();
        return typedStorage != nullptr ? typedStorage->tryGet(entity) : nullptr;
    }

    /// Returns the storage of `T`, creating it if needed.
    template <Component T>
    [[nodiscard]]
    ComponentStorage<T>& storage()
    {
        if (ComponentStorage<T>* existing = findStorage<T>()) {
            return *existing;
        }

        auto created = std::make_unique<ComponentStorage<T>>();
        ComponentStorage<T>& reference = *created;
        _storages.emplace(componentTypeId<T>(), std::move(created));
        return reference;
    }

    /// Returns the storage of `T`, or nullptr if no `T` was ever added.
    template <Component T>
    [[nodiscard]]
    ComponentStorage<T>* findStorage() noexcept
    {
        const auto found = _storages.find(componentTypeId<T>());
        if (found == _storages.end()) {
            return nullptr;
        }
        // Safe: the storage registered under T's id is always a ComponentStorage<T>.
        return static_cast<ComponentStorage<T>*>(found->second.get());
    }

    template <Component T>
    [[nodiscard]]
    const ComponentStorage<T>* findStorage() const noexcept
    {
        const auto found = _storages.find(componentTypeId<T>());
        if (found == _storages.end()) {
            return nullptr;
        }
        // Safe: the storage registered under T's id is always a ComponentStorage<T>.
        return static_cast<const ComponentStorage<T>*>(found->second.get());
    }

    /// Removes every component of `entity`, whatever their types.
    /// Called when an entity is destroyed.
    void removeAll(Entity entity);

    /// Removes every component of every entity. Storages are kept.
    void clear() noexcept;

private:
    std::unordered_map<ComponentTypeId, std::unique_ptr<IComponentStorage>> _storages;
};

} // namespace ecs
