#include "server/Spawn.hpp"

#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Components/ArenaExit.hpp"
#include "game/Components/Confinement.hpp"
#include "game/Components/Health.hpp"
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
            .collidesWith = game::layer::None, // Hurts nothing: what hits it says so
        });
    world.addComponent(ship, PlayerShip{.playerId = playerId});
    world.addComponent(ship, game::Health{.current = game::ship::MaxHealth});
    world.addComponent(ship,
        game::ConfinedToArena{.halfWidth = game::ship::HalfWidth, .halfHeight = game::ship::HalfHeight});
    world.addComponent(ship, weapons::ShipCannon);
    world.addComponent(ship, Trigger{}); // Pulled by PlayerControlSystem
    return ship;
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
            .collidesWith = game::layer::PlayerShip, // Crashing into a ship
        });
    world.addComponent(bydo, Bydo{.baseY = baseY, .age = 0.0F});
    world.addComponent(bydo, game::Health{.current = game::bydo::MaxHealth});
    world.addComponent(bydo, Damage{.amount = 1, .diesOnHit = true});
    world.addComponent(bydo, ScoreValue{.points = game::bydo::Points});
    world.addComponent(bydo, game::DespawnOutsideArena{});
    Weapon cannon = weapons::BydoCannon;
    cannon.cooldown = firstShotDelay;
    world.addComponent(bydo, cannon);
    world.addComponent(bydo, Trigger{}); // Aimed by BydoAimSystem
    return bydo;
}

ecs::Entity spawnProjectile(ecs::World& world, const Projectile& projectile, std::uint8_t owner,
    game::Position from, game::Velocity direction)
{
    const ecs::Entity entity = spawnNetworked(world, projectile.kind, owner, from,
        {.dx = direction.dx * projectile.speed, .dy = direction.dy * projectile.speed},
        {
            .halfWidth = projectile.halfWidth,
            .halfHeight = projectile.halfHeight,
            .layer = projectile.layer,
            .collidesWith = projectile.hits,
        });
    world.addComponent(entity, Damage{.amount = projectile.damage, .diesOnHit = true});
    world.addComponent(entity, game::DespawnOutsideArena{});
    return entity;
}

} // namespace server
