#pragma once

#include "ecs/System/System.hpp"

namespace game {

/// Moves every entity that has a `Position` and a `Velocity`:
/// `position += velocity × delta`. Ships now, Bydos and missiles later.
///
/// It runs in the fixed-step simulation, so the delta is always the same and
/// the result does not depend on the frame rate.
class MovementSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace game
