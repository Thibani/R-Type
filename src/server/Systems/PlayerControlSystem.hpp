#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Turns each player's `PlayerInput` into their ship's `Velocity`. Runs in the
/// simulation, after `TickSystem` and before `game::MovementSystem`.
///
/// The arrow keys give a direction; its length is normalised, so a diagonal
/// is not faster than a straight line. Opposite keys cancel out, and no key
/// (or no input received yet) stops the ship.
class PlayerControlSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
