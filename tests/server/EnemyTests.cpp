// The enemy roster (src/game/Enemies.hpp, src/server/Spawn.hpp): each enemy's
// rules and behaviour, on the server's own simulation.

#include "TestGame.hpp"
#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Bydo.hpp"
#include "game/Components/Collision.hpp"
#include "game/Components/Health.hpp"
#include "game/Components/Motion.hpp"
#include "game/Enemies.hpp"
#include "network/Protocol/Protocol.hpp"
#include "server/Components/Behaviour.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/NetEntity.hpp"
#include "server/Singleton.hpp"
#include "server/Spawn.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <tuple>
#include <vector>

using namespace network::protocol;
using server::Enemy;
using server::NetEntity;
using server::TeamScore;
using testing_game::Game;
using testing_game::playerMissile;
using testing_game::Step;

namespace {

struct Roster {
    EntityKind kind;
    ecs::Entity (*spawn)(ecs::World&, float, float);
    std::uint8_t health;
    std::uint32_t points;
    float halfWidth;
    float entryY{400.0F}; // Where it appears when asked for y = 400
};

const std::vector<Roster>& roster()
{
    static const std::vector<Roster> All{
        {.kind = EntityKind::Bydo,
            .spawn = server::spawnBydo,
            .health = game::bydo::MaxHealth,
            .points = game::bydo::Points,
            .halfWidth = game::bydo::HalfWidth},
        {.kind = EntityKind::PataPata,
            .spawn = server::spawnPataPata,
            .health = game::patapata::MaxHealth,
            .points = game::patapata::Points,
            .halfWidth = game::patapata::HalfWidth},
        {.kind = EntityKind::Bug,
            .spawn = server::spawnBug,
            .health = game::bug::MaxHealth,
            .points = game::bug::Points,
            .halfWidth = game::bug::HalfWidth},
        {.kind = EntityKind::Cancer,
            .spawn = server::spawnCancer,
            .health = game::cancer::MaxHealth,
            .points = game::cancer::Points,
            .halfWidth = game::cancer::HalfWidth},
        {.kind = EntityKind::Cruiser,
            .spawn = server::spawnCruiser,
            .health = game::cruiser::MaxHealth,
            .points = game::cruiser::Points,
            .halfWidth = game::cruiser::HalfWidth},
        {.kind = EntityKind::Hopper,
            .spawn = server::spawnHopper,
            .health = game::hopper::MaxHealth,
            .points = game::hopper::Points,
            .halfWidth = game::hopper::HalfWidth,
            .entryY = game::arena::Height - game::hopper::HalfHeight}, // On the floor
        {.kind = EntityKind::Spinner,
            .spawn = server::spawnSpinner,
            .health = game::spinner::MaxHealth,
            .points = game::spinner::Points,
            .halfWidth = game::spinner::HalfWidth},
        {.kind = EntityKind::Gunner,
            .spawn = server::spawnGunner,
            .health = game::gunner::MaxHealth,
            .points = game::gunner::Points,
            .halfWidth = game::gunner::HalfWidth},
        {.kind = EntityKind::Mine,
            .spawn = server::spawnMine,
            .health = game::mine::MaxHealth,
            .points = game::mine::Points,
            .halfWidth = game::mine::HalfWidth},
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
        EXPECT_FLOAT_EQ(position.y, enemy.entryY);
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
    EXPECT_LE(start.dy, (game::bug::VerticalAcceleration * Step) + 0.01F); // Turns smoothly

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
    angles.reserve(missiles.size());
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
    Game game; // No ship: no distance points, only the kill counts
    const ecs::Entity cruiser = server::spawnCruiser(game.world, 300.0F, 100.0F);
    const auto park
        = [&] { game.world.getComponent<game::Position>(cruiser) = {.x = 1200.0F, .y = 300.0F}; };

    for (std::uint8_t hit = 1; hit < game::cruiser::MaxHealth; ++hit) {
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

// --- The second wave of enemies -----------------------------------------------

namespace {

/// Hits `target` with player missiles until it dies; returns how many it took.
int shootDown(Game& game, ecs::Entity target)
{
    int shots = 0;
    while (game.world.isAlive(target) && shots < 300) {
        (void)playerMissile(game.world, game.world.getComponent<game::Position>(target));
        game.steps(1);
        ++shots;
    }
    return shots;
}

} // namespace

TEST(Enemies, HopperHopsAlongTheFloor)
{
    Game game;
    const ecs::Entity hopper = server::spawnHopper(game.world, 0.0F, 100.0F);
    const float floor = game::arena::Height - game::hopper::HalfHeight;

    float highest = floor;
    float lowest = floor;
    for (int i = 0; i < 180; ++i) { // 3 s: several hops
        game.steps(1);
        const float y = game.world.getComponent<game::Position>(hopper).y;
        highest = std::min(highest, y);
        lowest = std::max(lowest, y);
    }

    EXPECT_LT(highest, floor - 80.0F); // It jumps...
    EXPECT_LT(lowest, floor + 15.0F); // ...and lands back on the floor
    EXPECT_LT(game.world.getComponent<game::Position>(hopper).x, game::arena::Width);
}

TEST(Enemies, SpinnerFliesInLoops)
{
    Game game;
    const ecs::Entity spinner = server::spawnSpinner(game.world, 540.0F, 0.0F);

    float top = 540.0F;
    float bottom = 540.0F;
    for (int i = 0; i < 150; ++i) { // 2.5 s: more than a full loop at 3 rad/s
        game.steps(1);
        const float y = game.world.getComponent<game::Position>(spinner).y;
        top = std::min(top, y);
        bottom = std::max(bottom, y);
    }

    EXPECT_NEAR(bottom - top, 2.0F * game::spinner::LoopRadius, 15.0F);
    EXPECT_LT(game.world.getComponent<game::Position>(spinner).x, game::arena::Width - 200.0F);
}

TEST(Enemies, GunnerStopsFiresThenLeaves)
{
    Game game;
    (void)game.join(0);
    (void)game.world.removeComponent<game::Hitbox>(game.shipOf(0)); // Out of the way
    const ecs::Entity gunner = server::spawnGunner(game.world, 300.0F, 0.0F);

    game.seconds(3.0F);
    EXPECT_NEAR(game.world.getComponent<game::Position>(gunner).x, game::gunner::StopX, 10.0F);
    EXPECT_GE(game.ofKind(EntityKind::EnemyMissile).size(), game::gunner::Shots); // A burst

    game.seconds(game::gunner::HoldSeconds);
    EXPECT_LT(game.world.getComponent<game::Position>(gunner).x, game::gunner::StopX - 100.0F);
}

TEST(Enemies, MineBurstsIntoMissilesWhenDestroyed)
{
    Game game;
    const ecs::Entity mine = server::spawnMine(game.world, 500.0F, 0.0F);
    game.world.getComponent<game::Position>(mine).x = 1200.0F;

    EXPECT_EQ(shootDown(game, mine), game::mine::MaxHealth);

    const auto missiles = game.ofKind(EntityKind::EnemyMissile);
    ASSERT_EQ(missiles.size(), game::mine::BurstShots);
    int goingRight = 0;
    for (const ecs::Entity missile : missiles) {
        const auto& velocity = game.world.getComponent<game::Velocity>(missile);
        EXPECT_NEAR(std::hypot(velocity.dx, velocity.dy), game::mine::BurstSpeed, 1.0F);
        goingRight += velocity.dx > 1.0F ? 1 : 0;
    }
    EXPECT_GT(goingRight, 0); // All around, not only towards the ships
}

TEST(Enemies, AMineLeavingTheArenaDoesNotBurst)
{
    Game game;
    const ecs::Entity mine = server::spawnMine(game.world, 500.0F, 0.0F);

    game.seconds(30.0F);

    EXPECT_FALSE(game.world.isAlive(mine));
    EXPECT_TRUE(game.ofKind(EntityKind::EnemyMissile).empty());
}
