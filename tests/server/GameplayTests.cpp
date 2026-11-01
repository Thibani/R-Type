// The first-game rules (#54 – #59) on a plain World running the server's own
// simulation (installSimulation), step by step: no sockets.

#include "TestPlayers.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Bydo.hpp"
#include "game/Components/Health.hpp"
#include "game/Components/Motion.hpp"
#include "game/Enemies.hpp"
#include "game/Ship.hpp"
#include "network/Protocol/Packets.hpp"
#include "server/Components/Behaviour.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/NetEntity.hpp"
#include "server/Components/Player.hpp"
#include "server/Network.hpp"
#include "server/ServerSystems.hpp"
#include "server/Singleton.hpp"
#include "server/Spawn.hpp"
#include "server/Systems/PlayerShipSystem.hpp"
#include "server/Systems/ReplicationSystem.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <set>
#include <tuple>
#include <variant>
#include <vector>

using namespace network::protocol;
using server::Enemy;
using server::NetEntity;
using server::PlayerInput;
using server::PlayerShip;
using server::TeamScore;
using testing_players::addPlayer;

namespace {

constexpr float Step = 1.0F / 60.0F;

/// A World running the server's simulation, with its own clock.
class Game {
public:
    Game() { server::installSimulation(world); }

    void steps(int count)
    {
        for (int i = 0; i < count; ++i) {
            time = time.advanced(Step);
            world.update(time);
        }
    }

    void seconds(float duration) { steps(static_cast<int>(std::ceil(duration / Step))); }

    /// A connected player with a ship in its lane, as PlayerShipSystem makes it.
    ecs::Entity join(std::uint8_t playerId)
    {
        const ecs::Entity player = addPlayer(world, playerId);
        world.addComponent(player, PlayerInput{});
        ships.update(world, ecs::Time{});
        return player;
    }

    void press(ecs::Entity player, std::uint8_t mask)
    {
        auto& input = world.getComponent<PlayerInput>(player);
        input.mask = mask;
        input.received = true;
    }

    template <typename T>
    std::vector<ecs::Entity> all()
    {
        std::vector<ecs::Entity> found;
        world.query<T>([&](ecs::Entity entity, T& /*component*/) { found.push_back(entity); });
        return found;
    }

    std::vector<ecs::Entity> ofKind(EntityKind kind)
    {
        std::vector<ecs::Entity> found;
        world.query<NetEntity>([&](ecs::Entity entity, NetEntity& net) {
            if (net.kind == kind) {
                found.push_back(entity);
            }
        });
        return found;
    }

    ecs::Entity shipOf(std::uint8_t playerId)
    {
        ecs::Entity found = ecs::NullEntity;
        world.query<PlayerShip>([&](ecs::Entity entity, PlayerShip& ship) {
            if (ship.playerId == playerId) {
                found = entity;
            }
        });
        return found;
    }

    ecs::World world;
    ecs::Time time;
    server::PlayerShipSystem ships;
};

/// Projectiles as the server's weapons fire them.
ecs::Entity playerMissile(ecs::World& world, game::Position from)
{
    return server::spawnProjectile(
        world, server::weapons::ShipCannon.projectile, 0, from, {.dx = 1.0F, .dy = 0.0F});
}

ecs::Entity enemyMissile(ecs::World& world, game::Position from)
{
    return server::spawnProjectile(
        world, server::weapons::BydoCannon.projectile, NoPlayer, from, {.dx = -1.0F, .dy = 0.0F});
}

/// The reliable messages queued for `player` since the last call.
std::vector<Payload> queued(ecs::World& world, ecs::Entity player)
{
    std::vector<Payload> payloads;
    for (const Packet& packet :
        world.getComponent<server::NetClient>(player).channel.takeDue(server::TimePoint{})) {
        payloads.push_back(packet.payload);
    }
    return payloads;
}

template <typename T>
std::vector<T> only(const std::vector<Payload>& payloads)
{
    std::vector<T> found;
    for (const Payload& payload : payloads) {
        if (const T* message = std::get_if<T>(&payload)) {
            found.push_back(*message);
        }
    }
    return found;
}

} // namespace

// --- Player missiles (#54) ------------------------------------------------------

TEST(Gameplay, HoldingFireShootsAtTheCooldownRate)
{
    Game game;
    const ecs::Entity player = game.join(0);

    game.press(player, input::Fire);
    game.seconds(1.0F);

    // One at once, then one every 0.25 s: 4 or 5 in a second, never 60
    const auto missiles = game.ofKind(EntityKind::PlayerMissile);
    EXPECT_GE(missiles.size(), 4U);
    EXPECT_LE(missiles.size(), 5U);
    for (const ecs::Entity missile : missiles) {
        EXPECT_EQ(game.world.getComponent<NetEntity>(missile).owner, 0);
        EXPECT_GT(game.world.getComponent<game::Velocity>(missile).dx, 0.0F); // Flies right
    }
}

TEST(Gameplay, NoFireNoMissile)
{
    Game game;
    const ecs::Entity player = game.join(0);

    game.press(player, input::Right);
    game.seconds(1.0F);

    EXPECT_TRUE(game.ofKind(EntityKind::PlayerMissile).empty());
}

TEST(Gameplay, MissileLeavingTheArenaIsDespawned)
{
    Game game;
    const ecs::Entity player = game.join(0);
    game.press(player, input::Fire);
    game.steps(1);
    game.press(player, 0);

    game.seconds(2.0F); // 1200 units/s: off screen after ~1.6 s

    EXPECT_TRUE(game.ofKind(EntityKind::PlayerMissile).empty());
}

// --- Enemies (#55, #56, #57) -----------------------------------------------------

namespace {

/// An enemy owned by no player, on the right half of the arena.
::testing::AssertionResult isEnteringEnemy(ecs::World& world, ecs::Entity entity)
{
    const auto& position = world.getComponent<game::Position>(entity);
    const auto& net = world.getComponent<NetEntity>(entity);
    const bool notAnEnemy = net.kind == EntityKind::PlayerShip
        || net.kind == EntityKind::PlayerMissile || net.kind == EntityKind::EnemyMissile;
    if (notAnEnemy || net.owner != NoPlayer) {
        return ::testing::AssertionFailure() << "kind " << static_cast<int>(net.kind);
    }
    if (position.x < game::arena::Width / 2.0F || position.y < 0.0F
        || position.y > game::arena::Height) {
        return ::testing::AssertionFailure() << "at " << position.x << ", " << position.y;
    }
    return ::testing::AssertionSuccess();
}

} // namespace

TEST(Gameplay, NoEnemyWithoutPlayers)
{
    Game game;

    game.seconds(5.0F);

    EXPECT_TRUE(game.all<Enemy>().empty());
}

TEST(Gameplay, EnemiesEnterFromTheRightAtTheSpawnRate)
{
    Game game;
    (void)game.join(0);

    game.seconds(game::enemies::SpawnInterval * 3.0F + Step);

    const auto enemies = game.all<Enemy>();
    EXPECT_EQ(enemies.size(), 3U);
    for (const ecs::Entity enemy : enemies) {
        EXPECT_TRUE(isEnteringEnemy(game.world, enemy));
    }
}

TEST(Gameplay, BydoFliesLeftInAWave)
{
    Game game;
    const ecs::Entity bydo = server::spawnBydo(game.world, 540.0F, 100.0F); // Never fires

    float minY = 540.0F;
    float maxY = 540.0F;
    for (int i = 0; i < 180; ++i) { // 3 s: about one full wave
        game.steps(1);
        const float y = game.world.getComponent<game::Position>(bydo).y;
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }

    const auto& position = game.world.getComponent<game::Position>(bydo);
    EXPECT_NEAR(position.x, game::arena::Width + game::bydo::HalfWidth - 3.0F * game::bydo::Speed,
        2.0F);
    EXPECT_NEAR(maxY - 540.0F, game::bydo::WaveAmplitude, 5.0F);
    EXPECT_NEAR(540.0F - minY, game::bydo::WaveAmplitude, 5.0F);
}

TEST(Gameplay, BydoFiresAtTheNearestShipOnceOnScreen)
{
    Game game;
    (void)game.join(0);
    const auto ship = game.world.getComponent<game::Position>(game.shipOf(0));
    const ecs::Entity bydo = server::spawnBydo(game.world, ship.y, 0.0F); // Ready at once

    game.steps(1);
    EXPECT_TRUE(game.ofKind(EntityKind::EnemyMissile).empty()); // Still off screen

    game.seconds(0.5F); // Entered the arena: it fires
    const auto missiles = game.ofKind(EntityKind::EnemyMissile);
    ASSERT_EQ(missiles.size(), 1U);
    const auto& velocity = game.world.getComponent<game::Velocity>(missiles[0]);
    EXPECT_LT(velocity.dx, 0.0F); // Towards the ship, on the left
    EXPECT_NEAR(std::hypot(velocity.dx, velocity.dy), game::bydo::MissileSpeed, 1.0F);
    EXPECT_GT(game.world.getComponent<server::Weapon>(bydo).cooldown, 0.0F); // Reloading
}

// --- Collisions, damage and death (#58, #59) ---------------------------------------

TEST(Gameplay, PlayerMissileKillsABydoAndScores)
{
    Game game;
    (void)game.join(0);
    const ecs::Entity bydo = server::spawnBydo(game.world, 300.0F, 100.0F);
    game.world.getComponent<game::Position>(bydo) = {.x = 600.0F, .y = 300.0F};
    const ecs::Entity missile = playerMissile(game.world, {.x = 590.0F, .y = 300.0F});

    game.steps(1);

    EXPECT_FALSE(game.world.isAlive(bydo));
    EXPECT_FALSE(game.world.isAlive(missile)); // Used up
    EXPECT_EQ(server::singleton<TeamScore>(game.world).value, game::bydo::Points);
}

TEST(Gameplay, OneMissileKillsOneBydoOnly)
{
    Game game;
    (void)game.join(0);
    const ecs::Entity first = server::spawnBydo(game.world, 300.0F, 100.0F);
    const ecs::Entity second = server::spawnBydo(game.world, 300.0F, 100.0F);
    game.world.getComponent<game::Position>(first) = {.x = 600.0F, .y = 300.0F};
    game.world.getComponent<game::Position>(second) = {.x = 600.0F, .y = 300.0F};
    (void)playerMissile(game.world, {.x = 600.0F, .y = 300.0F});

    game.steps(1);

    EXPECT_EQ(game.world.isAlive(first) + game.world.isAlive(second), 1);
    EXPECT_EQ(server::singleton<TeamScore>(game.world).value, game::bydo::Points);
}

TEST(Gameplay, EnemyMissileCostsTheShipOneHeart)
{
    Game game;
    (void)game.join(0);
    const ecs::Entity ship = game.shipOf(0);
    const auto position = game.world.getComponent<game::Position>(ship);

    (void)enemyMissile(game.world, position);
    game.steps(1);

    EXPECT_EQ(game.world.getComponent<game::Health>(ship).current, game::ship::MaxHealth - 1);
    EXPECT_TRUE(game.ofKind(EntityKind::EnemyMissile).empty()); // Used up
}

TEST(Gameplay, ThreeHitsKillTheShipWhichComesBackLater)
{
    Game game;
    const ecs::Entity player = game.join(0);
    const ecs::Entity ship = game.shipOf(0);

    for (int hit = 0; hit < game::ship::MaxHealth; ++hit) {
        const auto position = game.world.getComponent<game::Position>(ship);
        (void)enemyMissile(game.world, position);
        game.steps(1);
    }

    EXPECT_FALSE(game.world.isAlive(ship));
    ASSERT_TRUE(game.world.hasComponent<server::RespawnTimer>(player));

    game.ships.update(game.world, ecs::Time{1.0F, 0.0F});
    EXPECT_EQ(game.shipOf(0), ecs::NullEntity); // Still waiting

    game.ships.update(game.world, ecs::Time{game::ship::RespawnDelay, 0.0F});
    const ecs::Entity reborn = game.shipOf(0);
    ASSERT_NE(reborn, ecs::NullEntity);
    EXPECT_EQ(game.world.getComponent<game::Health>(reborn).current, game::ship::MaxHealth);
}

TEST(Gameplay, RammingABydoKillsItAndCostsAHeart)
{
    Game game;
    (void)game.join(0);
    const ecs::Entity ship = game.shipOf(0);
    const ecs::Entity bydo = server::spawnBydo(game.world, 0.0F, 100.0F);
    game.world.getComponent<game::Position>(bydo) = game.world.getComponent<game::Position>(ship);

    game.steps(1);

    EXPECT_FALSE(game.world.isAlive(bydo));
    EXPECT_EQ(game.world.getComponent<game::Health>(ship).current, game::ship::MaxHealth - 1);
}

// --- Combat is data: any entity, any weapon ---------------------------------------

TEST(Gameplay, AnyEntityWithAWeaponAndATriggerFires)
{
    // No system knows about this "turret": the components are enough
    Game game;
    const ecs::Entity turret = game.world.createEntity();
    game.world.addComponent(turret, game::Position{.x = 960.0F, .y = 540.0F});
    game.world.addComponent(turret, NetEntity{.netId = 900, .kind = EntityKind::Bydo, .owner = NoPlayer});
    game.world.addComponent(turret, server::weapons::BydoCannon);
    game.world.addComponent(turret,
        server::Trigger{.pulled = true, .direction = {.dx = 0.0F, .dy = 1.0F}});

    game.steps(1);

    const auto missiles = game.ofKind(EntityKind::EnemyMissile);
    ASSERT_EQ(missiles.size(), 1U);
    const auto& velocity = game.world.getComponent<game::Velocity>(missiles[0]);
    EXPECT_FLOAT_EQ(velocity.dx, 0.0F);
    EXPECT_FLOAT_EQ(velocity.dy, game::bydo::MissileSpeed); // Along its trigger
}

TEST(Gameplay, DamageAmountComesFromTheProjectile)
{
    Game game;
    (void)game.join(0);
    const ecs::Entity ship = game.shipOf(0);
    server::Projectile heavy = server::weapons::BydoCannon.projectile;
    heavy.damage = game::ship::MaxHealth; // A one-shot weapon, as data only

    (void)server::spawnProjectile(
        game.world, heavy, NoPlayer, game.world.getComponent<game::Position>(ship), {});
    game.steps(1);

    EXPECT_FALSE(game.world.isAlive(ship));
}

// --- What the clients are told (#60) ------------------------------------------------

TEST(Gameplay, KillIsSentWithItsReasonKillerAndScore)
{
    Game game;
    const ecs::Entity player = game.join(0);
    server::ReplicationSystem replication(game.world);
    const ecs::Entity bydo = server::spawnBydo(game.world, 300.0F, 100.0F);
    replication.update(game.world, ecs::Time{});
    (void)queued(game.world, player); // Spawns and first GAME_STATE
    const auto bydoNetId = game.world.getComponent<NetEntity>(bydo).netId;

    game.world.getComponent<game::Position>(bydo) = {.x = 600.0F, .y = 300.0F};
    const ecs::Entity missile = playerMissile(game.world, {.x = 590.0F, .y = 300.0F});
    const auto missileNetId = game.world.getComponent<NetEntity>(missile).netId;
    replication.update(game.world, ecs::Time{}); // The missile's spawn
    game.steps(1);
    replication.update(game.world, ecs::Time{});

    const auto payloads = queued(game.world, player);
    const auto destroys = only<EntityDestroy>(payloads);
    const auto bydoDestroy = std::ranges::find(destroys, bydoNetId, &EntityDestroy::netId);
    ASSERT_NE(bydoDestroy, destroys.end());
    EXPECT_EQ(bydoDestroy->reason, DestroyReason::Killed);
    EXPECT_EQ(bydoDestroy->killerNetId, missileNetId);
    const auto states = only<GameState>(payloads);
    ASSERT_EQ(states.size(), 1U);
    EXPECT_EQ(states[0], (GameState{.phase = GamePhase::Running, .score = game::bydo::Points}));
}

TEST(Gameplay, ShipDeathIsSentAsPlayerDied)
{
    Game game;
    const ecs::Entity player = game.join(0);
    server::ReplicationSystem replication(game.world);
    replication.update(game.world, ecs::Time{});
    (void)queued(game.world, player);
    const ecs::Entity ship = game.shipOf(0);

    for (int hit = 0; hit < game::ship::MaxHealth; ++hit) {
        const auto position = game.world.getComponent<game::Position>(ship);
        (void)enemyMissile(game.world, position);
        game.steps(1);
    }
    replication.update(game.world, ecs::Time{});

    const auto deaths = only<PlayerDied>(queued(game.world, player));
    ASSERT_EQ(deaths.size(), 1U);
    EXPECT_EQ(deaths[0].playerId, 0);
}

TEST(Gameplay, OutOfBoundsIsSentAsSuch)
{
    Game game;
    const ecs::Entity player = game.join(0);
    server::ReplicationSystem replication(game.world);
    const ecs::Entity missile = playerMissile(game.world, {.x = 1900.0F, .y = 50.0F});
    const auto netId = game.world.getComponent<NetEntity>(missile).netId;
    replication.update(game.world, ecs::Time{});
    (void)queued(game.world, player);

    game.seconds(0.5F);
    replication.update(game.world, ecs::Time{});

    const auto destroys = only<EntityDestroy>(queued(game.world, player));
    const auto found = std::ranges::find(destroys, netId, &EntityDestroy::netId);
    ASSERT_NE(found, destroys.end());
    EXPECT_EQ(found->reason, DestroyReason::OutOfBounds);
}

TEST(Gameplay, NewcomerGetsTheScore)
{
    Game game;
    server::singleton<TeamScore>(game.world).value = 700;
    const ecs::Entity player = game.join(0);
    server::ReplicationSystem replication(game.world);

    replication.update(game.world, ecs::Time{});

    const auto states = only<GameState>(queued(game.world, player));
    ASSERT_EQ(states.size(), 1U);
    EXPECT_EQ(states[0].score, 700U);
}

// --- The enemy roster ------------------------------------------------------------

namespace {

struct Roster {
    EntityKind kind;
    ecs::Entity (*spawn)(ecs::World&, float, float);
    std::uint8_t health;
    std::uint32_t points;
    float halfWidth;
};

const std::vector<Roster>& roster()
{
    static const std::vector<Roster> All{
        {EntityKind::Bydo, server::spawnBydo, game::bydo::MaxHealth, game::bydo::Points,
            game::bydo::HalfWidth},
        {EntityKind::PataPata, server::spawnPataPata, game::patapata::MaxHealth,
            game::patapata::Points, game::patapata::HalfWidth},
        {EntityKind::Bug, server::spawnBug, game::bug::MaxHealth, game::bug::Points,
            game::bug::HalfWidth},
        {EntityKind::Cancer, server::spawnCancer, game::cancer::MaxHealth, game::cancer::Points,
            game::cancer::HalfWidth},
        {EntityKind::Cruiser, server::spawnCruiser, game::cruiser::MaxHealth, game::cruiser::Points,
            game::cruiser::HalfWidth},
    };
    return All;
}

/// The angle of a velocity, in radians.
float angleOf(const game::Velocity& velocity)
{
    return std::atan2(velocity.dy, velocity.dx);
}

} // namespace

TEST(Enemies, EveryEnemyEntersOnTheRightWithItsRules)
{
    for (const Roster& enemy : roster()) {
        Game game;
        const ecs::Entity entity = enemy.spawn(game.world, 400.0F, 1.0F);

        const auto& net = game.world.getComponent<NetEntity>(entity);
        const auto rules = std::tuple{static_cast<int>(net.kind), net.owner,
            game.world.hasComponent<Enemy>(entity),
            game.world.getComponent<game::Health>(entity).current,
            game.world.getComponent<server::ScoreValue>(entity).points,
            game.world.getComponent<game::Hitbox>(entity).layer};
        EXPECT_EQ(rules,
            (std::tuple{static_cast<int>(enemy.kind), NoPlayer, true, enemy.health, enemy.points,
                game::layer::Enemy}));
        const auto& position = game.world.getComponent<game::Position>(entity);
        EXPECT_FLOAT_EQ(position.x, game::arena::Width + enemy.halfWidth); // Just off screen
        EXPECT_FLOAT_EQ(position.y, 400.0F);
        EXPECT_LT(game.world.getComponent<game::Velocity>(entity).dx, 0.0F); // Flying left
    }
}

TEST(Enemies, EveryEnemyLeavesOnTheLeftIfNotShot)
{
    for (const Roster& enemy : roster()) {
        Game game;
        const ecs::Entity entity = enemy.spawn(game.world, 540.0F, 100.0F);

        game.seconds(40.0F); // Even the cruiser has crossed by then

        EXPECT_FALSE(game.world.isAlive(entity)) << static_cast<int>(enemy.kind);
        EXPECT_EQ(server::singleton<TeamScore>(game.world).value, 0U); // Not a kill
    }
}

TEST(Enemies, PataPataFliesInAFastWideWaveAndNeverFires)
{
    Game game;
    (void)game.join(0);
    const ecs::Entity drone = server::spawnPataPata(game.world, 540.0F, 0.0F);

    float minY = 540.0F;
    float maxY = 540.0F;
    for (int i = 0; i < 120; ++i) { // 2 s: more than one full wave at 4 rad/s
        game.steps(1);
        const float y = game.world.getComponent<game::Position>(drone).y;
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }

    EXPECT_NEAR(maxY - 540.0F, game::patapata::WaveAmplitude, 8.0F);
    EXPECT_NEAR(540.0F - minY, game::patapata::WaveAmplitude, 8.0F);
    EXPECT_FALSE(game.world.hasComponent<server::Weapon>(drone));
    EXPECT_TRUE(game.ofKind(EntityKind::EnemyMissile).empty());
}

TEST(Enemies, BugSteersTowardsTheShipWithinItsLimits)
{
    Game game;
    (void)game.join(0);
    const ecs::Entity ship = game.shipOf(0);
    game.world.getComponent<game::Position>(ship).y = 800.0F;
    const ecs::Entity bug = server::spawnBug(game.world, 200.0F, 0.0F);

    game.steps(1);
    const auto& start = game.world.getComponent<game::Velocity>(bug);
    EXPECT_FLOAT_EQ(start.dx, -game::bug::Speed);
    EXPECT_LE(start.dy, game::bug::VerticalAcceleration * Step + 0.01F); // Turns smoothly

    game.seconds(2.0F);
    const auto& velocity = game.world.getComponent<game::Velocity>(bug);
    EXPECT_FLOAT_EQ(velocity.dx, -game::bug::Speed); // Never turns back
    EXPECT_GT(velocity.dy, 0.0F); // Heading down, towards the ship
    EXPECT_LE(velocity.dy, game::bug::MaxVerticalSpeed);
    EXPECT_GT(game.world.getComponent<game::Position>(bug).y, 500.0F);
}

TEST(Enemies, BugFliesStraightWithoutShips)
{
    Game game;
    const ecs::Entity bug = server::spawnBug(game.world, 200.0F, 0.0F);

    game.seconds(1.0F);

    EXPECT_FLOAT_EQ(game.world.getComponent<game::Position>(bug).y, 200.0F);
}

TEST(Enemies, CancerFiresAFanOfThreeAimedMissiles)
{
    Game game;
    (void)game.join(0);
    const auto ship = game.world.getComponent<game::Position>(game.shipOf(0));
    const ecs::Entity cancer = server::spawnCancer(game.world, ship.y, 0.0F);
    game.world.getComponent<game::Position>(cancer).x = 1500.0F; // On screen, level with the ship

    game.steps(1);

    const auto missiles = game.ofKind(EntityKind::EnemyMissile);
    ASSERT_EQ(missiles.size(), game::cancer::Shots);
    std::vector<float> angles;
    for (const ecs::Entity missile : missiles) {
        angles.push_back(angleOf(game.world.getComponent<game::Velocity>(missile)));
    }
    std::ranges::sort(angles, [](float a, float b) {
        return std::abs(a) > std::abs(b); // The aimed one first: straight left, ±π
    });
    EXPECT_NEAR(std::abs(angles[0]), std::numbers::pi_v<float>, 0.05F); // Aimed at the ship
    EXPECT_NEAR(std::abs(angles[1]), std::numbers::pi_v<float> - game::cancer::Spread, 0.05F);
    EXPECT_NEAR(std::abs(angles[2]), std::numbers::pi_v<float> - game::cancer::Spread, 0.05F);
    EXPECT_NE(std::signbit(angles[1]), std::signbit(angles[2])); // One above, one below
}

TEST(Enemies, CruiserTakesFifteenHitsAndIsWorthAThousand)
{
    Game game;
    (void)game.join(0);
    const ecs::Entity cruiser = server::spawnCruiser(game.world, 300.0F, 100.0F);
    const auto park
        = [&] { game.world.getComponent<game::Position>(cruiser) = {.x = 1200.0F, .y = 300.0F}; };

    for (int hit = 1; hit < game::cruiser::MaxHealth; ++hit) {
        park();
        (void)playerMissile(game.world, {.x = 1200.0F, .y = 300.0F});
        game.steps(1);
    }
    ASSERT_TRUE(game.world.isAlive(cruiser));
    EXPECT_EQ(game.world.getComponent<game::Health>(cruiser).current, 1);
    EXPECT_EQ(server::singleton<TeamScore>(game.world).value, 0U);

    park();
    (void)playerMissile(game.world, {.x = 1200.0F, .y = 300.0F});
    game.steps(1);
    EXPECT_FALSE(game.world.isAlive(cruiser));
    EXPECT_EQ(server::singleton<TeamScore>(game.world).value, game::cruiser::Points);
}

TEST(Enemies, RammingACruiserDestroysTheShipNotTheCruiser)
{
    Game game;
    (void)game.join(0);
    const ecs::Entity ship = game.shipOf(0);
    const ecs::Entity cruiser = server::spawnCruiser(game.world, 300.0F, 100.0F);
    game.world.getComponent<game::Position>(cruiser)
        = game.world.getComponent<game::Position>(ship);

    game.steps(1);

    EXPECT_FALSE(game.world.isAlive(ship));
    ASSERT_TRUE(game.world.isAlive(cruiser));
    EXPECT_EQ(game.world.getComponent<game::Health>(cruiser).current, game::cruiser::MaxHealth);
}

TEST(Enemies, TheSpawnerSendsEveryKindOfEnemy)
{
    Game game;
    (void)game.join(0);
    game.world.removeComponent<game::Health>(game.shipOf(0)); // Invincible: the game goes on

    std::set<EntityKind> seen;
    for (int second = 0; second < 90; ++second) {
        game.seconds(1.0F);
        game.world.query<Enemy, NetEntity>([&](ecs::Entity /*entity*/, Enemy& /*enemy*/,
                                               NetEntity& net) { seen.insert(net.kind); });
    }

    EXPECT_EQ(seen,
        (std::set<EntityKind>{EntityKind::Bydo, EntityKind::PataPata, EntityKind::Bug,
            EntityKind::Cancer, EntityKind::Cruiser}));
}
