// The boss (src/game/Boss.hpp, BossSystem): when it comes, how it fights,
// what beating it gives, and the distance to it in GAME_STATE.

#include "TestGame.hpp"
#include "ecs/World/World.hpp"
#include "game/Boss.hpp"
#include "game/Components/Collision.hpp"
#include "game/Components/Health.hpp"
#include "game/Components/Motion.hpp"
#include "game/Enemies.hpp"
#include "game/Run.hpp"
#include "network/Protocol/Protocol.hpp"
#include "server/Components/Behaviour.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/Match.hpp"
#include "server/Components/NetEntity.hpp"
#include "server/Components/Waves.hpp"
#include "server/Run.hpp"
#include "server/Singleton.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

using namespace network::protocol;
using server::RunProgress;
using testing_game::Game;
using testing_game::playerMissile;

namespace {

/// A game with one ship that nothing touches, `secondsBefore` seconds of play
/// before the first boss.
class BossFight {
public:
    explicit BossFight(float secondsBefore = 0.5F)
    {
        (void)game.join(0);
        (void)game.world.removeComponent<game::Hitbox>(game.shipOf(0));
        progress().elapsed = game::boss::Interval - secondsBefore;
    }

    RunProgress& progress()
    {
        return server::singleton<RunProgress>(game.world);
    }

    /// The boss, or the null entity.
    ecs::Entity boss()
    {
        const auto bosses = game.all<server::Boss>();
        return bosses.empty() ? ecs::NullEntity : bosses.front();
    }

    Game game;
};

} // namespace

TEST(Boss, ComesAfterTheIntervalOfPlay)
{
    BossFight fight(1.0F);

    fight.game.seconds(0.5F);
    EXPECT_EQ(fight.boss(), ecs::NullEntity); // Not yet

    fight.game.seconds(1.0F);
    const ecs::Entity boss = fight.boss();
    ASSERT_NE(boss, ecs::NullEntity);
    EXPECT_EQ(fight.game.world.getComponent<server::NetEntity>(boss).kind, EntityKind::Boss);
    EXPECT_EQ(fight.game.world.getComponent<game::Health>(boss).current, game::boss::BaseHealth);
}

TEST(Boss, OnlyOneAtATime)
{
    BossFight fight;

    fight.game.seconds(10.0F);

    EXPECT_EQ(fight.game.all<server::Boss>().size(), 1U);
}

TEST(Boss, NoNewWaveWhileItIsThere)
{
    BossFight fight;
    fight.game.seconds(1.0F);
    ASSERT_NE(fight.boss(), ecs::NullEntity);
    const float untilNext = server::singleton<server::WaveDirector>(fight.game.world).untilNext;

    fight.game.seconds(10.0F);

    EXPECT_FLOAT_EQ(server::singleton<server::WaveDirector>(fight.game.world).untilNext, untilNext);
}

TEST(Boss, FliesInAndStaysOnTheRight)
{
    BossFight fight;

    fight.game.seconds(15.0F);

    const ecs::Entity boss = fight.boss();
    ASSERT_NE(boss, ecs::NullEntity);
    EXPECT_NEAR(fight.game.world.getComponent<game::Position>(boss).x, game::boss::StopX, 5.0F);
}

TEST(Boss, CyclesThroughItsAttacks)
{
    BossFight fight;
    fight.game.seconds(1.0F);
    const ecs::Entity boss = fight.boss();
    ASSERT_NE(boss, ecs::NullEntity);
    const auto attack
        = [&] { return fight.game.world.getComponent<server::AttackCycle>(boss).current; };
    EXPECT_EQ(attack(), server::BossAttack::Fan);

    fight.game.seconds(game::boss::AttackSeconds);
    EXPECT_EQ(attack(), server::BossAttack::Ring);

    const auto bugsBefore = fight.game.ofKind(EntityKind::Bug).size();
    fight.game.seconds(game::boss::AttackSeconds);
    EXPECT_EQ(attack(), server::BossAttack::Summon);
    EXPECT_EQ(fight.game.ofKind(EntityKind::Bug).size(), bugsBefore + game::boss::SummonCount);

    fight.game.seconds(game::boss::AttackSeconds);
    EXPECT_EQ(attack(), server::BossAttack::Fan); // And again
}

TEST(Boss, TheRingFiresAllAround)
{
    BossFight fight;
    fight.game.seconds(1.0F);
    const ecs::Entity boss = fight.boss();
    ASSERT_NE(boss, ecs::NullEntity);
    auto& cycle = fight.game.world.getComponent<server::AttackCycle>(boss);
    cycle.timeLeft = 0.0F; // Next step: Ring
    fight.game.world.getComponent<game::Position>(boss).x = game::boss::StopX; // On screen
    fight.game.world.getComponent<server::Weapon>(boss).cooldown = 0.0F;
    const auto before = fight.game.ofKind(EntityKind::EnemyMissile).size();

    fight.game.steps(1);

    const auto missiles = fight.game.ofKind(EntityKind::EnemyMissile);
    ASSERT_EQ(missiles.size(), before + game::boss::RingShots);
    int goingRight = 0;
    for (const ecs::Entity missile : missiles) {
        goingRight += fight.game.world.getComponent<game::Velocity>(missile).dx > 1.0F ? 1 : 0;
    }
    EXPECT_GE(goingRight, (static_cast<int>(game::boss::RingShots) / 2) - 1);
}

TEST(Boss, BelowHalfHealthItShootsFaster)
{
    BossFight fight;
    fight.game.seconds(1.0F);
    const ecs::Entity boss = fight.boss();
    ASSERT_NE(boss, ecs::NullEntity);
    const float calm = fight.game.world.getComponent<server::Weapon>(boss).interval;

    fight.game.world.getComponent<game::Health>(boss).current = game::boss::BaseHealth / 2 - 1;
    fight.game.steps(1);

    EXPECT_FLOAT_EQ(fight.game.world.getComponent<server::Weapon>(boss).interval,
        calm * game::boss::EnragedFactor);
}

TEST(Boss, BeatingItGivesPointsALifeAndTimeBeforeTheNext)
{
    BossFight fight;
    fight.game.seconds(15.0F);
    const ecs::Entity boss = fight.boss();
    ASSERT_NE(boss, ecs::NullEntity);
    auto& match = server::singleton<server::Match>(fight.game.world);
    match.lives = 2;
    const std::uint32_t scoreBefore = server::singleton<server::TeamScore>(fight.game.world).value;
    fight.game.world.getComponent<game::Health>(boss).current = 1;

    (void)playerMissile(fight.game.world, fight.game.world.getComponent<game::Position>(boss));
    fight.game.steps(1);

    EXPECT_FALSE(fight.game.world.isAlive(boss));
    EXPECT_EQ(server::singleton<server::Match>(fight.game.world).lives, 3);
    EXPECT_GE(server::singleton<server::TeamScore>(fight.game.world).value,
        scoreBefore + game::boss::Points);
    EXPECT_EQ(fight.progress().bossesDefeated, 1U);
    EXPECT_NEAR(
        fight.progress().nextBossAt, fight.progress().elapsed + game::boss::Interval, 0.05F);
}

TEST(Boss, LivesWonBackAreCapped)
{
    BossFight fight;
    fight.game.seconds(15.0F);
    const ecs::Entity boss = fight.boss();
    ASSERT_NE(boss, ecs::NullEntity);
    server::singleton<server::Match>(fight.game.world).lives = game::run::MaxLives;
    fight.game.world.getComponent<game::Health>(boss).current = 1;

    (void)playerMissile(fight.game.world, fight.game.world.getComponent<game::Position>(boss));
    fight.game.steps(1);

    EXPECT_EQ(server::singleton<server::Match>(fight.game.world).lives, game::run::MaxLives);
}

TEST(Boss, EachNewBossIsTougher)
{
    BossFight fight;
    fight.progress().bossesDefeated = 2;

    fight.game.seconds(1.0F);

    const ecs::Entity boss = fight.boss();
    ASSERT_NE(boss, ecs::NullEntity);
    EXPECT_EQ(fight.game.world.getComponent<game::Health>(boss).current,
        game::boss::BaseHealth + (2 * game::boss::HealthPerDefeat));
}

TEST(Boss, GameStateTellsTheDistanceToTheBoss)
{
    Game game;
    (void)game.join(0);
    auto& progress = server::singleton<RunProgress>(game.world);

    progress.elapsed = game::boss::Interval / 2.0F;
    EXPECT_EQ(server::currentState(game.world).bossProgress, 50);

    progress.elapsed = game::boss::Interval - 0.1F;
    EXPECT_EQ(server::currentState(game.world).bossProgress, 99); // 100 is for the boss itself

    (void)server::spawnBoss(game.world, 540.0F, game::boss::BaseHealth);
    EXPECT_EQ(server::currentState(game.world).bossProgress, MaxBossProgress);
}
