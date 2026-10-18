#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Flies every Bydo in a wave towards the left (#56):
///
///     x decreases at bydo::Speed
///     y = baseY + WaveAmplitude × sin(WaveFrequency × age)
///
/// It only sets the velocity (the derivative of that wave); `MovementSystem`
/// moves it, so clients extrapolate the same curve. Bydos that leave on the
/// left are despawned by `ArenaExitSystem` (OUT_OF_BOUNDS). Runs before
/// `game::MovementSystem`.
class BydoMovementSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
