#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// The players' "brain": turns each player's `PlayerInput` into their ship's
/// `Velocity`, and FIRE into its `Trigger` (`WeaponSystem` shoots). Runs in
/// the simulation, after `TickSystem` and before `WeaponSystem`.
///
/// The arrow keys give a direction; its length is normalised, so a diagonal
/// is not faster than a straight line. Opposite keys cancel out, and no key
/// (or no input received yet) stops the ship.
class PlayerControlSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
