#pragma once

#include "ecs/System/System.hpp"

namespace game {

/// Asks for the despawn of the entities that have left the arena
/// (`DespawnRequest` with the reason `OutOfBounds`).
///
/// Only entities with a `DespawnOutsideArena` are checked; see that component
/// for the exact limits. It runs after movement, on the positions of the step.
class ArenaExitSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace game
