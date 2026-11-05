// A game from start to game over (src/game/Run.hpp): shared lives, game over,
// restart, distance points, and what GAME_STATE tells the clients.

#include "TestGame.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Collision.hpp"
#include "game/Components/Motion.hpp"
#include "game/Run.hpp"
#include "game/Ship.hpp"
#include "game/Waves.hpp"
#include "network/Protocol/Packets.hpp"
#include "server/Components/Behaviour.hpp"
#include "server/Components/ClientMessage.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/Match.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/Player.hpp"
#include "server/Components/Waves.hpp"
#include "server/Network.hpp"
#include "server/Singleton.hpp"
#include "server/Spawn.hpp"
#include "server/Systems/ReplicationSystem.hpp"
#include "server/Systems/RestartSystem.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

using namespace network::protocol;
using server::Match;
using server::TeamScore;
using testing_game::enemyMissile;
using testing_game::Game;

namespace {

/// Shoots the ship of `playerId` down with enemy missiles.
void destroyShip(Game& game, std::uint8_t playerId)
{
    const ecs::Entity ship = game.shipOf(playerId);
    for (std::uint8_t hit = 0; hit < game::ship::MaxHealth; ++hit) {
        (void)enemyMissile(game.world, game.world.getComponent<game::Position>(ship));
        game.steps(1);
    }
    ASSERT_FALSE(game.world.isAlive(ship));
}

/// Lets the respawn delays run out, as `PlayerShipSystem` does every frame.
void waitForRespawn(Game& game)
{
    game.ships.update(game.world, ecs::Time{game::ship::RespawnDelay + 0.1F, 0.0F});
}

/// A RESTART from `player`, as SessionReceiveSystem delivers it.
void sendRestart(Game& game, ecs::Entity player)
{
    game.world.addComponent(
        game.world.createEntity(), server::ClientMessage{.client = player, .payload = Restart{}});
    server::RestartSystem{}.update(game.world, ecs::Time{});
}

/// The last GAME_STATE queued for `player`, if any.
std::optional<GameState> lastGameState(ecs::World& world, ecs::Entity player)
{
    std::optional<GameState> last;
    for (const Packet& packet :
        world.getComponent<server::NetClient>(player).channel.takeDue(server::TimePoint{})) {
        if (const auto* state = std::get_if<GameState>(&packet.payload)) {
            last = *state;
        }
    }
    return last;
}

/// Takes player 0's ship out of every collision: no kill, no ram, no score
/// but distance.
void untouchable(Game& game)
{
    (void)game.world.removeComponent<game::Hitbox>(game.shipOf(0));
}

Match& match(Game& game)
{
    return server::singleton<Match>(game.world);
}

} // namespace

// --- Lives -----------------------------------------------------------------------

TEST(Run, AGameStartsWithTheTeamsLives)
{
    Game game;
    (void)game.join(0);

    EXPECT_EQ(match(game).lives, game::run::StartingLives);
    EXPECT_EQ(match(game).phase, GamePhase::Running);
}

TEST(Run, ALostShipCostsALifeAndComesBack)
{
    Game game;
    const ecs::Entity player = game.join(0);

    destroyShip(game, 0);

    EXPECT_EQ(match(game).lives, game::run::StartingLives - 1);
    EXPECT_TRUE(game.world.hasComponent<server::RespawnTimer>(player));
    waitForRespawn(game);
    EXPECT_NE(game.shipOf(0), ecs::NullEntity);
}

TEST(Run, LosingTheLastLifeIsGameOver)
{
    Game game;
    const ecs::Entity player = game.join(0);

    for (std::uint8_t life = 0; life < game::run::StartingLives; ++life) {
        destroyShip(game, 0);
        waitForRespawn(game);
    }
    game.steps(1);

    EXPECT_EQ(match(game).lives, 0);
    EXPECT_TRUE(game.world.hasComponent<server::Eliminated>(player));
    EXPECT_EQ(match(game).phase, GamePhase::GameOver);
    EXPECT_EQ(game.shipOf(0), ecs::NullEntity); // No ship comes back
}

TEST(Run, TheGameGoesOnWhileATeammateStillFlies)
{
    Game game;
    (void)game.join(0);
    (void)game.join(1);
    match(game).lives = 1;

    destroyShip(game, 0);
    game.steps(1);

    EXPECT_EQ(match(game).phase, GamePhase::Running); // Player 1 is still there
    EXPECT_NE(game.shipOf(1), ecs::NullEntity);
}

TEST(Run, NoEnemiesAfterGameOver)
{
    Game game;
    (void)game.join(0);
    match(game).lives = 1;
    destroyShip(game, 0);
    game.steps(1);
    ASSERT_EQ(match(game).phase, GamePhase::GameOver);

    const auto before = game.all<server::Enemy>().size();
    game.seconds(10.0F);

    EXPECT_LE(game.all<server::Enemy>().size(), before); // Some fly away, none come
}

// --- Restart ---------------------------------------------------------------------

TEST(Run, RestartAfterGameOverStartsANewGame)
{
    Game game;
    const ecs::Entity player = game.join(0);
    match(game).lives = 1;
    server::singleton<TeamScore>(game.world).value = 4200;
    server::singleton<server::RunProgress>(game.world).elapsed = 100.0F;
    const ecs::Entity bydo = server::spawnBydo(game.world, 300.0F, 100.0F);
    destroyShip(game, 0);
    game.steps(1);
    ASSERT_EQ(match(game).phase, GamePhase::GameOver);

    sendRestart(game, player);
    game.steps(1); // The leftovers are despawned
    game.ships.update(game.world, ecs::Time{});

    EXPECT_EQ(match(game), Match{});
    EXPECT_EQ(server::singleton<TeamScore>(game.world).value, 0U);
    EXPECT_EQ(server::singleton<server::RunProgress>(game.world).level, 1U);
    EXPECT_FALSE(game.world.hasComponent<server::Eliminated>(player));
    EXPECT_NE(game.shipOf(0), ecs::NullEntity); // Back in the game
    EXPECT_FALSE(game.world.isAlive(bydo));
}

TEST(Run, RestartIsIgnoredWhileTheGameRuns)
{
    Game game;
    const ecs::Entity player = game.join(0);
    server::singleton<TeamScore>(game.world).value = 4200;

    sendRestart(game, player);

    EXPECT_EQ(server::singleton<TeamScore>(game.world).value, 4200U);
    EXPECT_TRUE(game.all<server::ClientMessage>().empty()); // Handled all the same
}

TEST(Run, WhenEveryoneLeftTheNextPlayerStartsAfresh)
{
    Game game;
    const ecs::Entity player = game.join(0);
    match(game).lives = 1;
    server::singleton<TeamScore>(game.world).value = 4200;

    game.world.destroyEntity(player); // Left
    game.steps(1);

    EXPECT_EQ(match(game), Match{});
    EXPECT_EQ(server::singleton<TeamScore>(game.world).value, 0U);
}

// --- Distance points -------------------------------------------------------------

TEST(Run, SurvivingEarnsDistancePoints)
{
    Game game;
    (void)game.join(0);
    untouchable(game);

    game.seconds(10.0F);

    const std::uint32_t score = server::singleton<TeamScore>(game.world).value;
    EXPECT_NEAR(static_cast<double>(score), 10.0 * game::run::DistancePointsPerSecond, 1.0);
}

TEST(Run, DistanceIsWorthMoreAtHigherLevels)
{
    Game game;
    (void)game.join(0);
    untouchable(game);
    server::singleton<server::RunProgress>(game.world).elapsed = 2.0F * game::waves::LevelDuration;

    game.seconds(10.0F); // Level 3 all along

    const std::uint32_t score = server::singleton<TeamScore>(game.world).value;
    EXPECT_NEAR(static_cast<double>(score), 3.0 * 10.0 * game::run::DistancePointsPerSecond, 1.0);
}

TEST(Run, NoDistancePointsWithoutShips)
{
    Game game;

    game.seconds(10.0F);

    EXPECT_EQ(server::singleton<TeamScore>(game.world).value, 0U);
}

// --- What the clients are told ---------------------------------------------------

TEST(Run, GameStateCarriesScoreLevelAndLives)
{
    Game game;
    const ecs::Entity player = game.join(0);
    server::ReplicationSystem replication(game.world);
    server::singleton<server::RunProgress>(game.world).level = 4;
    server::singleton<TeamScore>(game.world).value = 1234;

    replication.update(game.world, ecs::Time{});

    EXPECT_EQ(lastGameState(game.world, player),
        (GameState{.phase = GamePhase::Running,
            .score = 1234,
            .level = 4,
            .lives = game::run::StartingLives}));
}

TEST(Run, GameOverIsSentToTheClients)
{
    Game game;
    const ecs::Entity player = game.join(0);
    server::ReplicationSystem replication(game.world);
    match(game).lives = 1;

    destroyShip(game, 0);
    game.steps(1);
    replication.update(game.world, ecs::Time{});

    const GameState state = lastGameState(game.world, player).value_or(GameState{});
    EXPECT_EQ(state.phase, GamePhase::GameOver);
    EXPECT_EQ(state.lives, 0);
}

TEST(Run, ScoreAloneIsSentAtMostFourTimesASecond)
{
    Game game;
    const ecs::Entity player = game.join(0);
    server::ReplicationSystem replication(game.world);
    auto& score = server::singleton<TeamScore>(game.world).value;
    const auto frame = [&](float seconds) {
        replication.update(game.world, ecs::Time{0.0F, seconds});
        return lastGameState(game.world, player);
    };
    ASSERT_TRUE(frame(0.0F).has_value()); // The newcomer gets the state

    score = 10;
    EXPECT_TRUE(frame(0.1F).has_value()); // First score change: sent
    score = 20;
    EXPECT_FALSE(frame(0.2F).has_value()); // 100 ms later: waits
    match(game).lives = 1;
    EXPECT_TRUE(frame(0.25F).has_value()); // Lives changed: sent at once, score included
    score = 30;
    EXPECT_TRUE(frame(0.4F).has_value()); // 300 ms after the last score update
}
