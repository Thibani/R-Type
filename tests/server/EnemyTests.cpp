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
    Game game;
    (void)game.join(0);
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
