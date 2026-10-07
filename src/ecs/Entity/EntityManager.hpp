#pragma once

#include "ecs/Entity/Entity.hpp"

#include <cstddef>
#include <vector>

namespace ecs {

/// Creates, destroys and validates entity handles.
///
/// The manager only deals with identifiers: it knows nothing about
/// components. Removing the components of a destroyed entity is the
/// responsibility of the `World`, which coordinates both.
///
/// Ids of destroyed entities are recycled. Each time an id is recycled its
/// generation is incremented, so handles to the previous entity become stale
/// and `isAlive()` returns false for them.
class EntityManager {
public:
    /// Returns a handle to a new, alive entity.
    /// Reuses the id of a previously destroyed entity when one is available.
    [[nodiscard]]
    Entity create();

    /// Destroys an entity. Its handle (and every copy of it) becomes stale.
    /// Returns false if the entity was not alive (already destroyed or null).
    bool destroy(Entity entity);

    /// Returns true if the handle refers to an entity that is still alive.
    [[nodiscard]]
    bool isAlive(Entity entity) const noexcept;

    /// Number of entities currently alive.
    [[nodiscard]]
    std::size_t aliveCount() const noexcept;

    /// Destroys every entity. All existing handles become stale.
    void clear();

private:
    struct Slot {
        Entity::Generation generation{};
        bool alive{false};
    };

    std::vector<Slot> _slots; // Indexed by entity id
    std::vector<Entity::Id> _freeIds; // Ids of destroyed entities, ready for reuse
    std::size_t _aliveCount{0};
};

} // namespace ecs
