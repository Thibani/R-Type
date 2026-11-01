#pragma once

#include "ecs/Entity/Entity.hpp"
#include "game/Bydo.hpp"
#include "game/Components/Collision.hpp"
#include "game/Components/Motion.hpp"
#include "game/Enemies.hpp"
#include "game/Ship.hpp"
#include "network/Protocol/Protocol.hpp"
#include "server/Components/Combat.hpp"

#include <cstdint>

namespace ecs {
class World;
} // namespace ecs

/// Creates the networked gameplay entities with every component they need,
/// and a `NetEntity`, so `ReplicationSystem` announces them to every client.
///
/// Call them outside of any query (they create entities).
namespace server {

/// The weapons, as data: a new weapon (#86) is a new constant here, given to
/// an entity with a `Trigger`; `WeaponSystem` does the rest.
namespace weapons {

    inline constexpr Weapon ShipCannon{
        .projectile = {
            .kind = network::protocol::EntityKind::PlayerMissile,
            .speed = game::ship::MissileSpeed,
            .halfWidth = game::ship::MissileHalfWidth,
            .halfHeight = game::ship::MissileHalfHeight,
            .layer = game::layer::PlayerMissile,
            .hits = game::layer::Enemy,
            .damage = 1,
        },
        .interval = game::ship::FireCooldown,
        .cooldown = 0.0F,
        .muzzle = {.x = game::ship::HalfWidth, .y = 0.0F}, // The ship's nose
    };

    inline constexpr Weapon BydoCannon{
        .projectile = {
            .kind = network::protocol::EntityKind::EnemyMissile,
            .speed = game::bydo::MissileSpeed,
            .halfWidth = game::bydo::MissileHalfSize,
            .halfHeight = game::bydo::MissileHalfSize,
            .layer = game::layer::EnemyMissile,
            .hits = game::layer::PlayerShip,
            .damage = 1,
        },
        .interval = game::bydo::FireInterval,
        .cooldown = 0.0F,
        .muzzle = {},
    };

    /// A fan of 3 missiles.
    inline constexpr Weapon CancerFan{
        .projectile = {
            .kind = network::protocol::EntityKind::EnemyMissile,
            .speed = game::cancer::MissileSpeed,
            .halfWidth = game::bydo::MissileHalfSize,
            .halfHeight = game::bydo::MissileHalfSize,
            .layer = game::layer::EnemyMissile,
            .hits = game::layer::PlayerShip,
            .damage = 1,
        },
        .interval = game::cancer::FireInterval,
        .cooldown = 0.0F,
        .muzzle = {},
        .shots = game::cancer::Shots,
        .spread = game::cancer::Spread,
    };

    /// A fan of 5 missiles, from the cruiser's bow.
    inline constexpr Weapon CruiserBattery{
        .projectile = {
            .kind = network::protocol::EntityKind::EnemyMissile,
            .speed = game::cruiser::MissileSpeed,
            .halfWidth = game::bydo::MissileHalfSize,
            .halfHeight = game::bydo::MissileHalfSize,
            .layer = game::layer::EnemyMissile,
            .hits = game::layer::PlayerShip,
            .damage = 1,
        },
        .interval = game::cruiser::FireInterval,
        .cooldown = 0.0F,
        .muzzle = {.x = -game::cruiser::HalfWidth, .y = 0.0F},
        .shots = game::cruiser::Shots,
        .spread = game::cruiser::Spread,
    };

} // namespace weapons

ecs::Entity spawnPlayerShip(ecs::World& world, std::uint8_t playerId);

/// The enemies. Each one appears just outside the right edge at height `y`;
/// armed ones fire their first shot `firstShotDelay` seconds after entering
/// (unarmed ones ignore it). See `game/Enemies.hpp` for their rules.

/// Fighter flying in a gentle wave around `y`, firing aimed missiles.
ecs::Entity spawnBydo(ecs::World& world, float y, float firstShotDelay);

/// Drone flying in a fast, wide wave around `y`. Unarmed: it rams.
ecs::Entity spawnPataPata(ecs::World& world, float y, float firstShotDelay);

/// Insect steering towards the nearest ship. Unarmed: it rams.
ecs::Entity spawnBug(ecs::World& world, float y, float firstShotDelay);

/// Slow armoured crab (3 HP) firing aimed fans of 3 missiles.
ecs::Entity spawnCancer(ecs::World& world, float y, float firstShotDelay);

/// Heavy cruiser (15 HP) firing aimed fans of 5 missiles; ramming it kills.
ecs::Entity spawnCruiser(ecs::World& world, float y, float firstShotDelay);

/// A projectile fired by `owner` (a playerId, or `NoPlayer` for an enemy)
/// from `from`, flying along `direction` (a unit vector).
ecs::Entity spawnProjectile(ecs::World& world, const Projectile& projectile, std::uint8_t owner,
    game::Position from, game::Velocity direction);

} // namespace server
