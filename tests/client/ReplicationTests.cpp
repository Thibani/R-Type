#include "client/Components/Appearance.hpp"
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

TEST(Replication, StartSessionPlaysAtOnceWithoutGameState)
{
    // The server spawns the ship as soon as a player connects and does not
    // send GAME_STATE yet: one player alone must be able to play.
    ecs::World world;
    client::startSession(world,
        ConnectAccept{.playerId = 0, .tickRate = 60, .snapshotRate = 30, .maxPlayers = 4}, "Solo");
    const auto& session = client::singleton<client::Session>(world);
    EXPECT_EQ(session.screen, client::Screen::Game);
    EXPECT_EQ(session.playerId, 0);
    EXPECT_TRUE(session.players.at(0).present);
    EXPECT_EQ(session.players.at(0).name, "Solo");
}

TEST(Replication, GameStateSelectsTheScreen)
{
    ecs::World world;
    client::startSession(world,
        ConnectAccept{.playerId = 1, .tickRate = 60, .snapshotRate = 30, .maxPlayers = 4}, "B");
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
    client::startSession(world,
        ConnectAccept{.playerId = 0, .tickRate = 60, .snapshotRate = 30, .maxPlayers = 4}, "A");
    auto& session = client::singleton<client::Session>(world);

    client::applyPayload(world, PlayerJoined{.playerId = 3, .name = "D"}, 1.0F);
    EXPECT_TRUE(session.players.at(3).present);
    EXPECT_EQ(session.notice, "P4 D joined the game");

    client::applyPayload(world, PlayerDied{.playerId = 3, .killerNetId = 12}, 2.0F);
    EXPECT_TRUE(session.players.at(3).down);

    client::applyPayload(world, PlayerLeft{.playerId = 3, .reason = LeaveReason::Timeout}, 3.0F);
    EXPECT_FALSE(session.players.at(3).present);
    EXPECT_TRUE(session.players.at(3).left);
    EXPECT_EQ(session.notice, "P4 left the game (connection lost)");
    EXPECT_FLOAT_EQ(session.noticeUntil, 3.0F + client::NoticeSeconds);
}

TEST(Replication, ClearSessionForgetsNetworkEntitiesButKeepsTheStars)
{
    ecs::World world;
    client::spawnStarfield(world);
    const std::size_t stars = world.entityCount();
    client::applyPayload(
        world, snapshot(1, {shipState(1, 0.0F, 0.0F), shipState(2, 0.0F, 0.0F)}), 0.0F);

    client::clearSession(world);
    EXPECT_FALSE(entityOf(world, 1).has_value());
    // Stars + the singletons created on the way (Session, NetRegistry, InputHistory)
    EXPECT_EQ(world.entityCount(), stars + 3);
}
