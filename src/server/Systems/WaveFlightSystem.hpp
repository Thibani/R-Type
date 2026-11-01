#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Flies every entity with a `WaveFlight` towards the left, in a wave:
///
///     x decreases at speed
///     y = baseY + amplitude × sin(frequency × age)
///
/// It only sets the velocity (the derivative of that wave); `MovementSystem`
/// moves it, so clients extrapolate the same curve. Enemies that leave on the
/// left are despawned by `ArenaExitSystem` (OUT_OF_BOUNDS). Runs before
/// `game::MovementSystem`.
class WaveFlightSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
