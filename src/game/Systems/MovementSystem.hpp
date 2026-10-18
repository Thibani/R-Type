#pragma once

#include "ecs/System/System.hpp"

namespace game {

/// Moves every entity that has a `Position` and a `Velocity`:
///
///     position += velocity * deltaSeconds
///
/// Speeds are in world units per second. It runs in the fixed-step
/// simulation, so the delta is always the same and the result does not
/// depend on the frame rate. The system does not know what it moves: ships,
/// Bydos and missiles all move the same way.
class MovementSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace game
