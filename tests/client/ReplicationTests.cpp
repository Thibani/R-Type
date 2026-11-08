#include "client/Components/Appearance.hpp"
#include "client/Components/Controls.hpp"
#include "client/Components/NetEntity.hpp"
#include "client/Components/Session.hpp"
#include "client/Components/SoundEvent.hpp"
#include "client/Replication.hpp"
#include "client/Singleton.hpp"
#include "client/Systems/StarfieldSystem.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace network::protocol;

namespace {

std::optional<ecs::Entity> entityOf(ecs::World& world, std::uint16_t netId)
{
    const auto& entities = client::singleton<client::NetRegistry>(world).entities;
    const auto found = entities.find(netId);
    if (found == entities.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::size_t soundCount(ecs::World& world, client::Sound sound)
{
    std::size_t count = 0;
    world.query<client::SoundEvent>([&](ecs::Entity /*entity*/, client::SoundEvent& event) {
        count += event.sound == sound ? 1 : 0;
    });
    return count;
}

/// In the game as player `playerId`, called `name`: events only count there.
void inGame(ecs::World& world, std::uint8_t playerId = 0, const std::string& name = "Solo")
{
    client::singleton<client::Session>(world).name = name;
    client::enterGame(world, playerId);
}

EntityState shipState(std::uint16_t netId, float x, float y)
{
    return EntityState{.netId = netId,
        .kind = EntityKind::PlayerShip,
        .owner = 0,
        .health = 3,
        .x = quantize(x),
        .y = quantize(y),
        .vx = 0,
        .vy = 0};
}

Snapshot snapshot(std::uint32_t tick, std::vector<EntityState> entities)
{
    return Snapshot{.serverTick = tick,
        .lastInputTick = 0,
        .partIndex = 0,
        .partCount = 1,
        .entities = std::move(entities)};
}

} // namespace

TEST(Replication, SpawnCreatesAnEntityWithItsLook)
{
    ecs::World world;
    inGame(world);
    client::applyPayload(world,
        EntitySpawn{.netId = 7,
            .kind = EntityKind::Bydo,
            .owner = NoPlayer,
            .x = quantize(1500.0F),
            .y = quantize(300.5F),
            .vx = quantize(-100.0F),
            .vy = 0},
        0.0F);

    const auto entity = entityOf(world, 7);
    ASSERT_TRUE(entity.has_value());
    const auto& position = world.getComponent<game::Position>(*entity);
    EXPECT_FLOAT_EQ(position.x, 1500.0F);
    EXPECT_FLOAT_EQ(position.y, 300.5F);
    EXPECT_FLOAT_EQ(world.getComponent<game::Velocity>(*entity).dx, -100.0F);
    EXPECT_EQ(world.getComponent<client::Appearance>(*entity).look, client::Look::Bydo);
}

TEST(Replication, MissileSpawnPlaysTheShotSound)
{
    ecs::World world;
    inGame(world);
    client::applyPayload(world,
        EntitySpawn{.netId = 9,
            .kind = EntityKind::PlayerMissile,
            .owner = 1,
            .x = 0,
            .y = 0,
            .vx = 0,
            .vy = 0},
        0.0F);
    EXPECT_EQ(soundCount(world, client::Sound::Shot), 1U);
}

TEST(Replication, UnknownKindIsAPlaceholderNotACrash)
{
    ecs::World world;
    inGame(world);
    client::applyPayload(world,
        EntitySpawn{.netId = 3,
            .kind = static_cast<EntityKind>(42), // §5.2: reserved for new content
            .owner = NoPlayer,
            .x = 0,
            .y = 0,
            .vx = 0,
            .vy = 0},
        0.0F);
    const auto entity = entityOf(world, 3);
    ASSERT_TRUE(entity.has_value());
    EXPECT_EQ(world.getComponent<client::Appearance>(*entity).look, client::Look::Unknown);
}

TEST(Replication, SnapshotMovesKnownEntitiesAndCreatesMissingOnes)
{
    ecs::World world;
    inGame(world);
    client::applyPayload(world, snapshot(10, {shipState(1, 100.0F, 200.0F)}), 0.0F);
    const auto entity = entityOf(world, 1);
    ASSERT_TRUE(entity.has_value()); // §4.3: MAY create an entity from a snapshot

    client::applyPayload(world, snapshot(11, {shipState(1, 150.0F, 250.0F)}), 0.1F);
    EXPECT_FLOAT_EQ(world.getComponent<game::Position>(*entity).x, 150.0F);
    EXPECT_EQ(world.getComponent<client::NetEntity>(*entity).health, 3);
}

TEST(Replication, OlderSnapshotIsIgnored)
{
    ecs::World world;
    inGame(world);
    client::applyPayload(world, snapshot(20, {shipState(1, 100.0F, 100.0F)}), 0.0F);
    client::applyPayload(world, snapshot(19, {shipState(1, 999.0F, 999.0F)}), 0.1F);
    EXPECT_FLOAT_EQ(world.getComponent<game::Position>(*entityOf(world, 1)).x, 100.0F);

    // Another part of the same tick still counts
    client::applyPayload(world, snapshot(20, {shipState(2, 300.0F, 300.0F)}), 0.1F);
    EXPECT_TRUE(entityOf(world, 2).has_value());
}

TEST(Replication, DestroyedEntityIsNotRevivedByALateSnapshot)
{
    ecs::World world;
    inGame(world);
    client::applyPayload(world, snapshot(1, {shipState(5, 10.0F, 10.0F)}), 0.0F);
    client::applyPayload(world,
        EntityDestroy{.netId = 5, .reason = DestroyReason::Killed, .killerNetId = NoEntity}, 1.0F);
    EXPECT_FALSE(entityOf(world, 5).has_value());
    EXPECT_EQ(soundCount(world, client::Sound::Explosion), 1U);

    client::applyPayload(world, snapshot(2, {shipState(5, 10.0F, 10.0F)}), 2.0F);
    EXPECT_FALSE(entityOf(world, 5).has_value()); // §5.1: ignored for 5 seconds

    client::applyPayload(world, snapshot(3, {shipState(5, 10.0F, 10.0F)}), 6.5F);
    EXPECT_TRUE(entityOf(world, 5).has_value()); // Afterwards the netId may be reused
}

TEST(Replication, EnteringTheGamePlaysAtOnceWithoutGameState)
{
    // The server spawns the ship as soon as a player joins and does not
    // send GAME_STATE yet: one player alone must be able to play.
    ecs::World world;
    inGame(world, 0, "Solo");
    const auto& session = client::singleton<client::Session>(world);
    EXPECT_EQ(session.screen, client::Screen::Game);
    EXPECT_EQ(session.playerId, 0);
    EXPECT_TRUE(session.players.at(0).present);
    EXPECT_EQ(session.players.at(0).name, "Solo");
}

TEST(Replication, GameStateSelectsTheScreen)
{
    ecs::World world;
    inGame(world, 1, "B");
    auto& session = client::singleton<client::Session>(world);

    client::applyPayload(world, GameState{.phase = GamePhase::WaitingForPlayers, .score = 0}, 0.0F);
    EXPECT_EQ(session.screen, client::Screen::WaitingRoom);
    client::applyPayload(world, GameState{.phase = GamePhase::Running, .score = 120}, 0.0F);
    EXPECT_EQ(session.screen, client::Screen::Game);
    EXPECT_EQ(session.score, 120U);
    client::applyPayload(world, GameState{.phase = GamePhase::GameOver, .score = 12450}, 0.0F);
    EXPECT_EQ(session.screen, client::Screen::GameOver);
    EXPECT_EQ(session.score, 12450U);
}

TEST(Replication, PlayerEventsUpdateTheSlots)
{
    ecs::World world;
    inGame(world, 0, "A");
    auto& session = client::singleton<client::Session>(world);
    client::applyPayload(world, GameState{.phase = GamePhase::Running, .score = 0}, 0.5F);

    client::applyPayload(world, PlayerJoined{.playerId = 3, .name = "D"}, 1.0F);
    EXPECT_TRUE(session.players.at(3).present);
    EXPECT_EQ(session.notice, "P4 D joined the game");

    client::applyPayload(world, PlayerDied{.playerId = 3, .killerNetId = 12}, 2.0F);
    EXPECT_TRUE(session.players.at(3).down);

    client::applyPayload(world, PlayerLeft{.playerId = 3, .reason = LeaveReason::Timeout}, 3.0F);
    EXPECT_FALSE(session.players.at(3).present);
    EXPECT_TRUE(session.players.at(3).left);
    EXPECT_EQ(session.notice, "P4 D left the game (connection lost)");
    EXPECT_FLOAT_EQ(session.noticeUntil, 3.0F + client::NoticeSeconds);
}

TEST(Replication, PlayersAlreadyThereAreShownButNotAnnounced)
{
    // §8.1: a newcomer first receives PLAYER_JOINED for every player already
    // connected. They fill the HUD; only later arrivals get a notice and a sound.
    ecs::World world;
    inGame(world, 1, "B");
    auto& session = client::singleton<client::Session>(world);

    client::applyPayload(world, PlayerJoined{.playerId = 0, .name = "A"}, 0.1F);
    client::applyPayload(world, snapshot(1, {shipState(1, 0.0F, 0.0F)}), 0.1F);
    client::applyPayload(world, PlayerJoined{.playerId = 2, .name = "C"}, 0.2F);
    EXPECT_TRUE(session.players.at(0).present);
    EXPECT_TRUE(session.players.at(2).present);
    EXPECT_TRUE(session.notice.empty());
    EXPECT_EQ(soundCount(world, client::Sound::PlayerJoined), 0U);

    client::applyPayload(world, GameState{.phase = GamePhase::Running, .score = 0}, 0.3F);
    client::applyPayload(world, PlayerJoined{.playerId = 3, .name = "D"}, 5.0F);
    EXPECT_EQ(session.notice, "P4 D joined the game");
    EXPECT_EQ(soundCount(world, client::Sound::PlayerJoined), 1U);
}

TEST(Replication, ClearSessionForgetsNetworkEntitiesButKeepsTheStars)
{
    ecs::World world;
    client::spawnStarfield(world);
    const std::size_t stars = world.entityCount();
    inGame(world);
    client::applyPayload(
        world, snapshot(1, {shipState(1, 0.0F, 0.0F), shipState(2, 0.0F, 0.0F)}), 0.0F);

    client::clearSession(world);
    EXPECT_FALSE(entityOf(world, 1).has_value());
    // Stars + the singletons created on the way (Session, NetRegistry, InputHistory)
    EXPECT_EQ(world.entityCount(), stars + 3);
}

// --- Main page and game (§8.4) --------------------------------------------------

TEST(MainPage, LoggingInOpensTheMainPage)
{
    ecs::World world;
    client::startSession(world);

    const auto& session = client::singleton<client::Session>(world);
    EXPECT_EQ(session.screen, client::Screen::Home);
    EXPECT_FALSE(session.playerId.has_value());
}

TEST(MainPage, EventsOfTheGameAreIgnored)
{
    ecs::World world;
    client::startSession(world);
    const std::size_t before = world.entityCount();

    client::applyPayload(world, snapshot(1, {shipState(1, 0.0F, 0.0F)}), 0.0F);
    client::applyPayload(world, GameState{.phase = GamePhase::Running, .score = 50}, 0.0F);
    client::applyPayload(world, PlayerJoined{.playerId = 1, .name = "B"}, 0.0F);

    const auto& session = client::singleton<client::Session>(world);
    EXPECT_EQ(world.entityCount(), before);
    EXPECT_EQ(session.screen, client::Screen::Home);
    EXPECT_EQ(session.score, 0U);
    EXPECT_FALSE(session.players.at(1).present);
}

TEST(MainPage, AcceptedJoinEntersTheGame)
{
    ecs::World world;
    client::startSession(world);
    auto& session = client::singleton<client::Session>(world);
    session.name = "Dave";

    client::applyPayload(world, JoinResult{.code = JoinCode::Ok, .playerId = 2}, 0.0F);

    EXPECT_EQ(session.screen, client::Screen::Game);
    EXPECT_EQ(session.playerId, 2);
    EXPECT_EQ(session.players.at(2).name, "Dave");
}

TEST(MainPage, RefusedStartStaysOnThePageWithAMessage)
{
    ecs::World world;
    client::startSession(world);
    auto& session = client::singleton<client::Session>(world);

    client::applyPayload(world, JoinResult{.code = JoinCode::NotReady, .playerId = NoPlayer}, 0.0F);

    EXPECT_EQ(session.screen, client::Screen::Home);
    ASSERT_EQ(session.chat.size(), 1U);
    EXPECT_EQ(session.chat[0].kind, client::ChatLine::Kind::Notice);
    EXPECT_FALSE(session.playerId.has_value());
}

TEST(MainPage, TheGroupIsKeptAsTheServerTellsIt)
{
    ecs::World world;
    client::startSession(world);
    auto& session = client::singleton<client::Session>(world);
    session.name = "Max";
    EXPECT_TRUE(client::leadsTheGroup(session)); // Alone, not told yet

    const GroupState state{.code = "ABC234",
        .countdown = 0,
        .leader = 0,
        .members = {{.name = "Lea", .ready = false, .inGame = false},
            {.name = "Max", .ready = true, .inGame = false}}};
    client::applyPayload(world, state, 0.0F);

    EXPECT_EQ(session.group, state);
    EXPECT_FALSE(client::leadsTheGroup(session));
}

TEST(MainPage, TheSameInvitationCountsOnce)
{
    ecs::World world;
    client::startSession(world);
    auto& session = client::singleton<client::Session>(world);

    client::applyPayload(world, GroupInvited{.name = "Lea"}, 0.0F);
    client::applyPayload(world, GroupInvited{.name = "Lea"}, 0.0F);
    client::applyPayload(world, GroupInvited{.name = "Zoe"}, 0.0F);

    EXPECT_EQ(session.invitations, (std::vector<std::string>{"Lea", "Zoe"}));
}

TEST(MainPage, GroupResultsAreToldInTheChat)
{
    ecs::World world;
    client::startSession(world);
    auto& session = client::singleton<client::Session>(world);

    client::applyPayload(world, GroupResult{.outcome = GroupOutcome::UnknownCode}, 0.0F);

    ASSERT_EQ(session.chat.size(), 1U);
    EXPECT_EQ(session.chat[0].text, "No group has this code.");
}

TEST(MainPage, LoggingOutForgetsTheGroup)
{
    ecs::World world;
    client::startSession(world);
    auto& session = client::singleton<client::Session>(world);
    session.group.code = "ABC234";
    session.groupCodeInput = "XYZ";
    session.invitations.push_back("Lea");

    client::clearSession(world);

    EXPECT_TRUE(session.group.code.empty());
    EXPECT_TRUE(session.groupCodeInput.empty());
    EXPECT_TRUE(session.invitations.empty());
}

TEST(MainPage, LeavingForgetsTheGameButNotTheSession)
{
    ecs::World world;
    inGame(world);
    client::applyPayload(world, snapshot(10, {shipState(1, 0.0F, 0.0F)}), 0.0F);
    client::applyPayload(world, GameState{.phase = GamePhase::Running, .score = 50}, 0.0F);
    client::singleton<client::InputHistory>(world).firstTick = 500;

    client::leaveGame(world);

    const auto& session = client::singleton<client::Session>(world);
    EXPECT_EQ(session.screen, client::Screen::Home);
    EXPECT_FALSE(session.playerId.has_value());
    EXPECT_EQ(session.score, 0U);
    EXPECT_FALSE(entityOf(world, 1).has_value());
    EXPECT_EQ(client::singleton<client::InputHistory>(world).firstTick, 500U);
}

TEST(MainPage, ALateSnapshotOfTheLastGameIsNotApplied)
{
    ecs::World world;
    inGame(world);
    client::applyPayload(world, snapshot(10, {shipState(1, 0.0F, 0.0F)}), 0.0F);
    client::leaveGame(world);
    inGame(world);

    client::applyPayload(world, snapshot(9, {shipState(1, 0.0F, 0.0F)}), 0.0F);

    EXPECT_FALSE(entityOf(world, 1).has_value());
}

TEST(MainPage, LoggingOutForgetsTheSessionToo)
{
    ecs::World world;
    inGame(world);
    client::applyPayload(world, snapshot(10, {shipState(1, 0.0F, 0.0F)}), 0.0F);
    client::singleton<client::InputHistory>(world).firstTick = 500;

    client::clearSession(world);

    EXPECT_FALSE(client::singleton<client::NetRegistry>(world).newestTick.has_value());
    EXPECT_EQ(client::singleton<client::InputHistory>(world).firstTick, 0U);
}

// --- Leaderboard (§8.8) ------------------------------------------------------------

TEST(Leaderboard, TheAnswerForTheTabShownIsKept)
{
    ecs::World world;
    client::startSession(world);
    auto& session = client::singleton<client::Session>(world);
    session.board = Board::Multi;
    const Leaderboard multi{.board = Board::Multi,
        .best = {{.score = 900, .names = {"Ann", "Ben"}}},
        .lastRank = 3,
        .lastScore = 40};

    client::applyPayload(world, multi, 0.0F);

    ASSERT_TRUE(session.leaderboard.has_value());
    EXPECT_EQ(*session.leaderboard, multi);
}

TEST(Leaderboard, AnAnswerForTheOtherTabIsIgnored)
{
    ecs::World world;
    client::startSession(world);
    auto& session = client::singleton<client::Session>(world);
    session.board = Board::Multi; // Solo was asked first, then the player switched

    client::applyPayload(
        world, Leaderboard{.board = Board::Solo, .best = {}, .lastRank = 0, .lastScore = 0}, 0.0F);

    EXPECT_FALSE(session.leaderboard.has_value());
}

TEST(Leaderboard, ItArrivesDuringAGameToo)
{
    ecs::World world;
    inGame(world);

    client::applyPayload(
        world, Leaderboard{.board = Board::Solo, .best = {}, .lastRank = 0, .lastScore = 0}, 0.0F);

    EXPECT_TRUE(client::singleton<client::Session>(world).leaderboard.has_value());
}

TEST(Leaderboard, EnteringAGameClosesIt)
{
    ecs::World world;
    client::startSession(world);
    auto& session = client::singleton<client::Session>(world);
    session.leaderboardOpen = true; // A member reads it while the leader starts the game

    client::applyPayload(world, JoinResult{.code = JoinCode::Ok, .playerId = 1}, 0.0F);

    EXPECT_EQ(session.screen, client::Screen::Game);
    EXPECT_FALSE(session.leaderboardOpen);
}

TEST(Leaderboard, LoggingOutForgetsIt)
{
    ecs::World world;
    client::startSession(world);
    auto& session = client::singleton<client::Session>(world);
    session.leaderboardOpen = true;
    session.board = Board::Multi;
    session.leaderboard
        = Leaderboard{.board = Board::Multi, .best = {}, .lastRank = 0, .lastScore = 0};

    client::clearSession(world);

    EXPECT_FALSE(session.leaderboardOpen);
    EXPECT_EQ(session.board, Board::Solo);
    EXPECT_FALSE(session.leaderboard.has_value());
}

// --- Friends (§8.5) -----------------------------------------------------------------

TEST(Friends, TheListKeepsItsOrder)
{
    ecs::World world;
    client::startSession(world);
    client::applyPayload(
        world, FriendEntry{.name = "Sent", .state = FriendState::RequestSent}, 0.0F);
    client::applyPayload(world, FriendEntry{.name = "Zed", .state = FriendState::Offline}, 0.0F);
    client::applyPayload(world, FriendEntry{.name = "Ann", .state = FriendState::Offline}, 0.0F);
    client::applyPayload(world, FriendEntry{.name = "On", .state = FriendState::Online}, 0.0F);
    client::applyPayload(
        world, FriendEntry{.name = "Asks", .state = FriendState::RequestReceived}, 0.0F);

    std::vector<std::string> names;
    for (const client::FriendRow& row : client::singleton<client::Session>(world).friends) {
        names.push_back(row.name);
    }
    EXPECT_EQ(names, (std::vector<std::string>{"Asks", "On", "Ann", "Zed", "Sent"}));
}

TEST(Friends, AChangeReplacesTheRowAndRemovedForgetsIt)
{
    ecs::World world;
    client::startSession(world);
    const auto& friends = client::singleton<client::Session>(world).friends;

    client::applyPayload(world, FriendEntry{.name = "Ann", .state = FriendState::Offline}, 0.0F);
    client::applyPayload(world, FriendEntry{.name = "Ann", .state = FriendState::Online}, 0.0F);
    ASSERT_EQ(friends.size(), 1U);
    EXPECT_EQ(friends.at(0).state, FriendState::Online);

    client::applyPayload(world, FriendEntry{.name = "Ann", .state = FriendState::Removed}, 0.0F);
    EXPECT_TRUE(friends.empty());
}

TEST(Friends, TheListLivesThroughAGameButNotALogOut)
{
    ecs::World world;
    inGame(world);
    client::applyPayload(world, FriendEntry{.name = "Ann", .state = FriendState::Online}, 0.0F);
    const auto& session = client::singleton<client::Session>(world);
    ASSERT_EQ(session.friends.size(), 1U);

    client::leaveGame(world);
    EXPECT_EQ(session.friends.size(), 1U);

    client::clearSession(world);
    EXPECT_TRUE(session.friends.empty());
}

TEST(Friends, AFriendComingOnlineDoesNotEndTheCatchingUp)
{
    // Otherwise a player already in the game would be announced as joining
    ecs::World world;
    inGame(world, 1, "B");
    auto& session = client::singleton<client::Session>(world);

    client::applyPayload(world, FriendEntry{.name = "Ann", .state = FriendState::Online}, 0.1F);
    client::applyPayload(world, PlayerJoined{.playerId = 0, .name = "A"}, 0.1F);

    EXPECT_TRUE(session.notice.empty());
}

TEST(Friends, ResultsSayWhatHappened)
{
    ecs::World world;
    client::startSession(world);
    auto& session = client::singleton<client::Session>(world);
    session.friendName = "Ann";

    client::applyPayload(world, FriendResult{.code = FriendCode::UnknownName}, 0.0F);
    EXPECT_EQ(session.friendMessage, "No player has this name.");
    EXPECT_EQ(session.friendName, "Ann"); // A typing mistake can be corrected

    client::applyPayload(world, FriendResult{.code = FriendCode::Ok}, 0.0F);
    EXPECT_TRUE(session.friendName.empty());
}

// --- Endless mode (#145) --------------------------------------------------------------

TEST(Replication, EveryEnemyKindHasItsOwnLook)
{
    const std::vector<std::pair<EntityKind, client::Look>> enemies{
        {EntityKind::Bydo, client::Look::Bydo},
        {EntityKind::PataPata, client::Look::PataPata},
        {EntityKind::Bug, client::Look::Bug},
        {EntityKind::Cancer, client::Look::Cancer},
        {EntityKind::Cruiser, client::Look::Cruiser},
        {EntityKind::Hopper, client::Look::Hopper},
        {EntityKind::Spinner, client::Look::Spinner},
        {EntityKind::Gunner, client::Look::Gunner},
        {EntityKind::Mine, client::Look::Mine},
        {EntityKind::Boss, client::Look::Boss},
    };
    for (const auto& [kind, look] : enemies) {
        EXPECT_EQ(client::lookOf(kind), look) << static_cast<int>(kind);
    }
}

TEST(Replication, EveryGadgetHasItsOwnLook)
{
    EXPECT_EQ(client::lookOf(EntityKind::Force), client::Look::Force);
    EXPECT_EQ(client::lookOf(EntityKind::ForceCapsule), client::Look::ForceCapsule);
    EXPECT_EQ(client::lookOf(EntityKind::RepairKit), client::Look::RepairKit);
}

TEST(Replication, GameStateBringsLevelAndLives)
{
    ecs::World world;
    inGame(world, 0, "A");
    auto& session = client::singleton<client::Session>(world);

    client::applyPayload(
        world, GameState{.phase = GamePhase::Running, .score = 300, .level = 4, .lives = 2}, 0.0F);
    EXPECT_EQ(session.level, 4);
    EXPECT_EQ(session.lives, 2);

    // Play again: a new game starts back at level 1 with every life
    client::applyPayload(
        world, GameState{.phase = GamePhase::GameOver, .score = 300, .level = 4, .lives = 0}, 1.0F);
    client::applyPayload(
        world, GameState{.phase = GamePhase::Running, .score = 0, .level = 1, .lives = 3}, 2.0F);
    EXPECT_EQ(session.screen, client::Screen::Game);
    EXPECT_EQ(session.level, 1);
    EXPECT_EQ(session.lives, 3);

    client::clearSession(world);
    EXPECT_EQ(session.level, 1);
    EXPECT_EQ(session.lives, 0);
}

TEST(Replication, TheBossKeepsItsFullHealthForTheHealthBar)
{
    ecs::World world;
    inGame(world, 0, "A");
    client::applyPayload(world,
        EntitySpawn{.netId = 9,
            .kind = EntityKind::Boss,
            .owner = NoPlayer,
            .x = 0,
            .y = 0,
            .vx = 0,
            .vy = 0},
        1.0F);
    EXPECT_EQ(client::singleton<client::Session>(world).notice, "Warning: a boss is coming!");

    EntityState boss{.netId = 9,
        .kind = EntityKind::Boss,
        .owner = NoPlayer,
        .health = 160,
        .x = 0,
        .y = 0,
        .vx = 0,
        .vy = 0};
    client::applyPayload(world, snapshot(1, {boss}), 1.0F);
    boss.health = 40;
    client::applyPayload(world, snapshot(2, {boss}), 2.0F);

    const auto entity = entityOf(world, 9);
    ASSERT_TRUE(entity.has_value());
    const auto& data = world.getComponent<client::NetEntity>(entity.value_or(ecs::NullEntity));
    EXPECT_EQ(data.health, 40);
    EXPECT_EQ(data.peakHealth, 160); // A quarter of its health left
    EXPECT_EQ(world.getComponent<client::Appearance>(entity.value_or(ecs::NullEntity)).look,
        client::Look::Boss);
}

TEST(Replication, GameStateBringsTheDistanceToTheBoss)
{
    ecs::World world;
    inGame(world);
    client::applyPayload(world,
        GameState{
            .phase = GamePhase::Running, .score = 0, .level = 2, .lives = 3, .bossProgress = 64},
        0.0F);

    EXPECT_EQ(client::singleton<client::Session>(world).bossProgress, 64);
}
