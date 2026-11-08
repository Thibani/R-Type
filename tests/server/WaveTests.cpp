// The endless mode (src/game/Waves.hpp, WaveSystem, ProgressSystem): levels,
// arrivals that speed up, enemies and formations that unlock, on the server's
// own simulation.

#include "TestGame.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Health.hpp"
#include "game/Components/Motion.hpp"
#include "game/Waves.hpp"
#include "network/Protocol/Protocol.hpp"
#include "server/Components/Behaviour.hpp"
#include "server/Components/NetEntity.hpp"
#include "server/Components/Waves.hpp"
#include "server/Singleton.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

using namespace network::protocol;
using server::Enemy;
using server::NetEntity;
using server::RunProgress;
using server::WaveDirector;
using testing_game::Game;

namespace {

/// A game with one invincible ship, so the waves never stop, starting at
/// `level`.
class EndlessRun {
public:
    explicit EndlessRun(std::uint32_t level = 1)
    {
        (void)game.join(0);
        game.world.removeComponent<game::Health>(game.shipOf(0)); // Never killed
        auto& progress = server::singleton<RunProgress>(game.world);
        progress.elapsed = static_cast<float>(level - 1) * game::waves::LevelDuration;
        progress.nextBossAt = 1.0e9F; // Only waves here: bosses have their own tests
    }

    /// Plays `seconds`, recording every enemy that entered.
    void play(float seconds)
    {
        const int steps = static_cast<int>(seconds / testing_game::Step);
        for (int i = 0; i < steps; ++i) {
            game.steps(1);
            game.world.query<Enemy, NetEntity, game::Position>(
                [&](ecs::Entity /*entity*/, const Enemy& /*enemy*/, const NetEntity& net,
                    const game::Position& position) {
                    if (seenNetIds.insert(net.netId).second) {
                        arrivals.emplace_back(net.kind, position.y);
                        kinds.insert(net.kind);
                    }
                });
            maxPending
                = std::max(maxPending, server::singleton<WaveDirector>(game.world).pending.size());
        }
    }

    [[nodiscard]]
    std::uint32_t level()
    {
        return server::singleton<RunProgress>(game.world).level;
    }

    Game game;
    std::set<std::uint16_t> seenNetIds;
    std::vector<std::pair<EntityKind, float>> arrivals;
    std::set<EntityKind> kinds;
    std::size_t maxPending{};
};

} // namespace

// --- Levels --------------------------------------------------------------------

TEST(Waves, LevelsLastThirtySeconds)
{
    EXPECT_EQ(game::waves::levelAt(0.0F), 1U);
    EXPECT_EQ(game::waves::levelAt(29.9F), 1U);
    EXPECT_EQ(game::waves::levelAt(30.0F), 2U);
    EXPECT_EQ(game::waves::levelAt(95.0F), 4U);
}

TEST(Waves, ArrivalsComeFasterEachLevelDownToAMinimum)
{
    for (std::uint32_t level = 1; level < 20; ++level) {
        EXPECT_GE(game::waves::spawnInterval(level), game::waves::spawnInterval(level + 1));
    }
    EXPECT_FLOAT_EQ(game::waves::spawnInterval(1), game::waves::BaseSpawnInterval);
    EXPECT_FLOAT_EQ(game::waves::spawnInterval(50), game::waves::MinSpawnInterval);
}

TEST(Waves, FormationsGetMoreLikelyUpToACap)
{
    EXPECT_FLOAT_EQ(game::waves::formationChance(1), 0.1F);
    EXPECT_FLOAT_EQ(game::waves::formationChance(3), 0.3F);
    EXPECT_FLOAT_EQ(game::waves::formationChance(40), game::waves::MaxFormationChance);
}

TEST(Waves, TheLevelRisesWithPlayTime)
{
    EndlessRun run;
    EXPECT_EQ(run.level(), 1U);

    run.play(game::waves::LevelDuration + 1.0F);

    EXPECT_EQ(run.level(), 2U);
}

TEST(Waves, TheClockStopsWithoutShips)
{
    Game game;

    game.seconds(60.0F);

    EXPECT_FLOAT_EQ(server::singleton<RunProgress>(game.world).elapsed, 0.0F);
    EXPECT_TRUE(game.all<Enemy>().empty());
}

// --- What comes ------------------------------------------------------------------

TEST(Waves, LevelOneOnlySendsBydosAndPataPatas)
{
    EndlessRun run;

    run.play(game::waves::LevelDuration - 1.0F);

    EXPECT_FALSE(run.kinds.empty());
    for (const EntityKind kind : run.kinds) {
        EXPECT_TRUE(kind == EntityKind::Bydo || kind == EntityKind::PataPata)
            << static_cast<int>(kind);
    }
}

TEST(Waves, LaterLevelsSendEveryKindOfEnemy)
{
    EndlessRun run(5);

    run.play(90.0F);

    EXPECT_EQ(run.kinds,
        (std::set<EntityKind>{EntityKind::Bydo, EntityKind::PataPata, EntityKind::Bug,
            EntityKind::Cancer, EntityKind::Cruiser, EntityKind::Hopper, EntityKind::Spinner,
            EntityKind::Gunner, EntityKind::Mine}));
}

TEST(Waves, MoreEnemiesComeAtHigherLevels)
{
    EndlessRun beginner(1);
    EndlessRun veteran(8);

    beginner.play(25.0F);
    veteran.play(25.0F);

    EXPECT_GT(veteran.arrivals.size(), beginner.arrivals.size() * 3 / 2);
}

TEST(Waves, FormationsBringSeveralEnemiesInARow)
{
    EndlessRun run(5); // A formation every other arrival

    run.play(30.0F);

    EXPECT_GE(run.maxPending, 2U); // Members waiting for their turn
}

TEST(Waves, TheSameSeedGivesTheSameWaves)
{
    EndlessRun first(3);
    EndlessRun second(3);

    first.play(40.0F);
    second.play(40.0F);

    ASSERT_FALSE(first.arrivals.empty());
    EXPECT_EQ(first.arrivals, second.arrivals);
}
