#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Fires every weapon whose `Trigger` is pulled, once its cooldown is over:
/// one projectile from the muzzle, along the trigger's direction, owned by
/// the shooter (`spawnProjectile`). Runs in the simulation, after the systems
/// that pull triggers and before movement.
///
/// It does not know who shoots: a ship, a Bydo, or anything given a `Weapon`
/// and a `Trigger`. The projectile's ENTITY_SPAWN, with the shooter as
/// `owner`, is how the clients learn that someone fired.
class WeaponSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
