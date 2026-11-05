#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Flies every `CircleFlight` in loops while drifting left: the velocity is
/// the drift plus the derivative of a circle of `radius`,
///
///     dx = −speed − radius × loopSpeed × sin(angle)
///     dy =          radius × loopSpeed × cos(angle)
///
/// so `MovementSystem` (and the clients' extrapolation) draw the loops.
/// Runs before `game::MovementSystem`.
class CircleFlightSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
