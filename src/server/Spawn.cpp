#include "server/Spawn.hpp"

#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Components/ArenaExit.hpp"
#include "game/Components/Confinement.hpp"
#include "game/Components/Health.hpp"
#include "server/Components/Behaviour.hpp"
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

namespace {

/// What every enemy has in common: where it enters, its size, its health,
/// what it is worth, and how it hurts a ship it crashes into.
struct EnemyTraits {
    EntityKind kind{};
    float y{};
    float halfWidth{};
    float halfHeight{};
    float speed{}; // Towards the left, until a behaviour changes it
    std::uint8_t health{};
    std::uint32_t points{};
    Damage ram;
};

ecs::Entity spawnEnemy(ecs::World& world, const EnemyTraits& traits)
{
    const ecs::Entity enemy = spawnNetworked(world, traits.kind, NoPlayer,
        {.x = game::arena::Width + traits.halfWidth, .y = traits.y},
        {.dx = -traits.speed, .dy = 0.0F},
        {
            .halfWidth = traits.halfWidth,
            .halfHeight = traits.halfHeight,
            .layer = game::layer::Enemy,
            .collidesWith = game::layer::PlayerShip, // Crashing into a ship
        });
    world.addComponent(enemy, Enemy{});
    world.addComponent(enemy, game::Health{.current = traits.health});
    world.addComponent(enemy, traits.ram);
    world.addComponent(enemy, ScoreValue{.points = traits.points});
    world.addComponent(enemy, game::DespawnOutsideArena{});
    return enemy;
}

/// Gives `enemy` a weapon aimed at the ships, ready `firstShotDelay` seconds
/// after it enters the arena.
void arm(ecs::World& world, ecs::Entity enemy, Weapon weapon, float firstShotDelay)
{
    weapon.cooldown = firstShotDelay;
    world.addComponent(enemy, weapon);
    world.addComponent(enemy, Trigger{});
    world.addComponent(enemy, AimsAtShips{});
}

} // namespace

ecs::Entity spawnBydo(ecs::World& world, float y, float firstShotDelay)
{
    using namespace game::bydo;
    const ecs::Entity bydo = spawnEnemy(world,
        {.kind = EntityKind::Bydo,
            .y = y,
            .halfWidth = HalfWidth,
            .halfHeight = HalfHeight,
            .speed = Speed,
            .health = MaxHealth,
            .points = Points,
            .ram = Damage{.amount = 1, .diesOnHit = true}});
    world.addComponent(bydo,
        WaveFlight{
            .baseY = y, .speed = Speed, .amplitude = WaveAmplitude, .frequency = WaveFrequency});
    arm(world, bydo, weapons::BydoCannon, firstShotDelay);
    return bydo;
}

ecs::Entity spawnPataPata(ecs::World& world, float y, float /*firstShotDelay*/)
{
    using namespace game::patapata;
    const ecs::Entity drone = spawnEnemy(world,
        {.kind = EntityKind::PataPata,
            .y = y,
            .halfWidth = HalfWidth,
            .halfHeight = HalfHeight,
            .speed = Speed,
            .health = MaxHealth,
            .points = Points,
            .ram = Damage{.amount = 1, .diesOnHit = true}});
    world.addComponent(drone,
        WaveFlight{
            .baseY = y, .speed = Speed, .amplitude = WaveAmplitude, .frequency = WaveFrequency});
    return drone;
}

ecs::Entity spawnBug(ecs::World& world, float y, float /*firstShotDelay*/)
{
    using namespace game::bug;
    const ecs::Entity bug = spawnEnemy(world,
        {.kind = EntityKind::Bug,
            .y = y,
            .halfWidth = HalfWidth,
            .halfHeight = HalfHeight,
            .speed = Speed,
            .health = MaxHealth,
            .points = Points,
            .ram = Damage{.amount = 1, .diesOnHit = true}});
    world.addComponent(bug,
        Seeker{.speed = Speed,
            .maxVerticalSpeed = MaxVerticalSpeed,
            .verticalAcceleration = VerticalAcceleration});
    return bug;
}

ecs::Entity spawnCancer(ecs::World& world, float y, float firstShotDelay)
{
    using namespace game::cancer;
    const ecs::Entity cancer = spawnEnemy(world,
        {.kind = EntityKind::Cancer,
            .y = y,
            .halfWidth = HalfWidth,
            .halfHeight = HalfHeight,
            .speed = Speed,
            .health = MaxHealth,
            .points = Points,
            .ram = Damage{.amount = 1, .diesOnHit = true}});
    world.addComponent(cancer,
        WaveFlight{
            .baseY = y, .speed = Speed, .amplitude = WaveAmplitude, .frequency = WaveFrequency});
    arm(world, cancer, weapons::CancerFan, firstShotDelay);
    return cancer;
}

ecs::Entity spawnCruiser(ecs::World& world, float y, float firstShotDelay)
{
    using namespace game::cruiser;
    const ecs::Entity cruiser = spawnEnemy(world,
        {.kind = EntityKind::Cruiser,
            .y = y,
            .halfWidth = HalfWidth,
            .halfHeight = HalfHeight,
            .speed = Speed,
            .health = MaxHealth,
            .points = Points,
            .ram
            = Damage{.amount = RamDamage, .diesOnHit = false}}); // The ship breaks, not the cruiser
    // No behaviour: it keeps flying straight at its initial speed
    arm(world, cruiser, weapons::CruiserBattery, firstShotDelay);
    return cruiser;
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
