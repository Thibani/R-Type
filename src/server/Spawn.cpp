#include "server/Spawn.hpp"

#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Bydo.hpp"
#include "game/Components/ArenaExit.hpp"
#include "game/Components/Collision.hpp"
#include "game/Components/Health.hpp"
#include "game/Ship.hpp"
#include "network/Protocol/Protocol.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/NetEntity.hpp"
#include "server/Components/Player.hpp"
#include "server/NetIds.hpp"

namespace server {

using namespace network::protocol;

namespace {

ecs::Entity spawnNetworked(ecs::World& world, EntityKind kind, std::uint8_t owner,
    game::Position position, game::Velocity velocity, game::Hitbox hitbox)
{
    const ecs::Entity entity = world.createEntity();
    world.addComponent(entity, position);
    world.addComponent(entity, velocity);
    world.addComponent(entity, hitbox);
    world.addComponent(entity, NetEntity{.netId = allocateNetId(world), .kind = kind, .owner = owner});
    return entity;
}

} // namespace

ecs::Entity spawnPlayerShip(ecs::World& world, std::uint8_t playerId)
{
    const auto spawn = game::arena::playerSpawn(playerId);
    const ecs::Entity ship = spawnNetworked(world, EntityKind::PlayerShip, playerId,
        {.x = spawn.x, .y = spawn.y}, {},
        {
            .halfWidth = game::ship::HalfWidth,
            .halfHeight = game::ship::HalfHeight,
            .layer = game::layer::PlayerShip,
            .collidesWith = game::layer::Enemy | game::layer::EnemyMissile,
        });
    world.addComponent(ship, PlayerShip{.playerId = playerId});
    world.addComponent(ship, game::Health{.current = game::ship::MaxHealth});
    world.addComponent(ship, FireCooldown{});
    return ship;
}

ecs::Entity spawnPlayerMissile(ecs::World& world, std::uint8_t playerId, game::Position muzzle)
{
    const ecs::Entity missile = spawnNetworked(world, EntityKind::PlayerMissile, playerId, muzzle,
        {.dx = game::ship::MissileSpeed, .dy = 0.0F},
        {
            .halfWidth = game::ship::MissileHalfWidth,
            .halfHeight = game::ship::MissileHalfHeight,
            .layer = game::layer::PlayerMissile,
            .collidesWith = game::layer::Enemy,
        });
    world.addComponent(missile, game::DespawnOutsideArena{});
    return missile;
}

ecs::Entity spawnBydo(ecs::World& world, float baseY, float firstShotDelay)
{
    const ecs::Entity bydo = spawnNetworked(world, EntityKind::Bydo, NoPlayer,
        {.x = game::arena::Width + game::bydo::HalfWidth, .y = baseY},
        {.dx = -game::bydo::Speed, .dy = 0.0F},
        {
            .halfWidth = game::bydo::HalfWidth,
            .halfHeight = game::bydo::HalfHeight,
            .layer = game::layer::Enemy,
            .collidesWith = game::layer::PlayerShip | game::layer::PlayerMissile,
        });
    world.addComponent(bydo, Bydo{.baseY = baseY, .age = 0.0F, .fireCooldown = firstShotDelay});
    world.addComponent(bydo, game::Health{.current = game::bydo::MaxHealth});
    world.addComponent(bydo, game::DespawnOutsideArena{});
    return bydo;
}

ecs::Entity spawnEnemyMissile(ecs::World& world, game::Position muzzle, game::Velocity velocity)
{
    const ecs::Entity missile = spawnNetworked(world, EntityKind::EnemyMissile, NoPlayer, muzzle,
        velocity,
        {
            .halfWidth = game::bydo::MissileHalfSize,
            .halfHeight = game::bydo::MissileHalfSize,
            .layer = game::layer::EnemyMissile,
            .collidesWith = game::layer::PlayerShip,
        });
    world.addComponent(missile, game::DespawnOutsideArena{});
    return missile;
}

} // namespace server
