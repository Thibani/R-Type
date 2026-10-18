#pragma once

#include "ecs/System/System.hpp"

namespace game {

/// Moves every entity that has a `Position` and a `Velocity`:
///
///     position += velocity * deltaSeconds
///
/// Speeds are in world units per second, so the movement does not depend on
/// the frame rate. The system does not know what it moves: ships, enemies and
/// missiles all move the same way.
class MovementSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace game
