#pragma once

#include "ecs/Entity/Entity.hpp"
#include "game/Components/Motion.hpp"

#include <cstdint>

namespace ecs {
class World;
} // namespace ecs

/// Creates the networked gameplay entities with every component they need:
/// position, velocity, hitbox, health, despawn rules and a `NetEntity`, so
/// `ReplicationSystem` announces them to every client.
///
/// Call them outside of any query (they create entities).
namespace server {

ecs::Entity spawnPlayerShip(ecs::World& world, std::uint8_t playerId);

/// A missile fired by player `playerId`'s ship, from `muzzle`, flying right.
ecs::Entity spawnPlayerMissile(ecs::World& world, std::uint8_t playerId, game::Position muzzle);

/// A Bydo just outside the right edge, flying in a wave around `baseY`.
ecs::Entity spawnBydo(ecs::World& world, float baseY, float firstShotDelay);

/// A Bydo missile from `muzzle`, flying at `velocity`.
ecs::Entity spawnEnemyMissile(ecs::World& world, game::Position muzzle, game::Velocity velocity);

} // namespace server
