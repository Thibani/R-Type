#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Fires the missiles of the players holding FIRE (#54): one every
/// `ship::FireCooldown` seconds per ship, from its nose, flying right.
/// Runs in the simulation, after `PlayerControlSystem`.
///
/// The missile is a networked entity (`spawnPlayerMissile`): its ENTITY_SPAWN,
/// with the shooter as `owner`, is how the clients learn that a player fired.
class PlayerFireSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
