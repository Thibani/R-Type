#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// The enemies' "brain" for shooting: each entity with `AimsAtShips` points
/// its `Trigger` at the nearest ship and pulls it, but only once it is inside
/// the arena (no shots from off screen). No ship in the game: no shot.
/// `WeaponSystem` fires, at the rate of the enemy's weapon. Runs before
/// `WeaponSystem`.
class AimSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
