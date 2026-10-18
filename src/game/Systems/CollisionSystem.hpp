#pragma once

#include "ecs/System/System.hpp"

namespace game {

/// Detects overlapping hitboxes and emits a `CollisionEvent` for each pair.
///
/// It looks at every entity that has a `Position` and a `Hitbox`, and checks
/// every pair: their boxes must overlap (touching edges do not count) and
/// their layers must match (`Hitbox::collidesWith`).
///
/// It only detects: it never applies damage nor destroys anything. Other
/// systems subscribe to `CollisionEvent` and decide what a collision means.
///
/// It runs after movement and confinement, so it sees the final positions of
/// the step. Each pair is checked once (O(n²)): about 100 entities on screen
/// means about 5000 cheap box tests per step, far below the 16 ms budget.
class CollisionSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace game
