#pragma once

#include "ecs/Entity/Entity.hpp"

namespace game {

/// Two entities' hitboxes overlap (see `CollisionSystem`).
///
/// Emitted on the World's EventBus once per overlapping pair and per
/// simulation step, as long as they overlap. `first` is always the entity
/// with the smaller id, so the order is the same on every run.
///
/// It only states the fact: what happens next (damage, death, pick-up) is
/// decided by the systems that listen to it.
struct CollisionEvent {
    ecs::Entity first;
    ecs::Entity second;
};

} // namespace game
