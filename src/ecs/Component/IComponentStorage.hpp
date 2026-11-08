#pragma once

#include "ecs/Entity/Entity.hpp"

#include <cstddef>

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

} // namespace ecs
