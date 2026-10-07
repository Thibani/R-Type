#include "ecs/Entity/EntityManager.hpp"

#include <stdexcept>

namespace ecs {

Entity EntityManager::create()
{
    Entity::Id id{};

    if (!_freeIds.empty()) {
        id = _freeIds.back();
        _freeIds.pop_back();
    } else {
        if (_slots.size() >= Entity::NullId) {
            throw std::length_error("EntityManager: maximum number of entities reached");
        }
        id = static_cast<Entity::Id>(_slots.size());
        _slots.emplace_back();
    }

    Slot& slot = _slots[id];
    slot.alive = true;
    ++_aliveCount;

    return Entity{id, slot.generation};
}

bool EntityManager::destroy(Entity entity)
{
    if (!isAlive(entity)) {
        return false;
    }

    Slot& slot = _slots[entity.id()];
    slot.alive = false;
    ++slot.generation; // Invalidates every existing handle to this entity
    _freeIds.push_back(entity.id());
    --_aliveCount;

    return true;
}

bool EntityManager::isAlive(Entity entity) const noexcept
{
    if (!entity.valid() || entity.id() >= _slots.size()) {
        return false;
    }

    const Slot& slot = _slots[entity.id()];
    return slot.alive && slot.generation == entity.generation();
}

std::size_t EntityManager::aliveCount() const noexcept
{
    return _aliveCount;
}

void EntityManager::clear()
{
    _freeIds.clear();
    _freeIds.reserve(_slots.size());

    // Push ids in reverse order so that they are reused from 0 upward.
    for (auto id = static_cast<Entity::Id>(_slots.size()); id > 0; --id) {
        Slot& slot = _slots[id - 1];
        if (slot.alive) {
            slot.alive = false;
            ++slot.generation;
        }
        _freeIds.push_back(id - 1);
    }

    _aliveCount = 0;
}

} // namespace ecs
