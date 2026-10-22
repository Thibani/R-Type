#pragma once

#include "ecs/Entity/Entity.hpp"
#include "game/Bydo.hpp"
#include "game/Components/Collision.hpp"
#include "game/Components/Motion.hpp"
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

} // namespace weapons

ecs::Entity spawnPlayerShip(ecs::World& world, std::uint8_t playerId);

/// A Bydo just outside the right edge, flying in a wave around `baseY`.
ecs::Entity spawnBydo(ecs::World& world, float baseY, float firstShotDelay);

/// A projectile fired by `owner` (a playerId, or `NoPlayer` for an enemy)
/// from `from`, flying along `direction` (a unit vector).
ecs::Entity spawnProjectile(ecs::World& world, const Projectile& projectile, std::uint8_t owner,
    game::Position from, game::Velocity direction);

} // namespace server
