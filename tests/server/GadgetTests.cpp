// The gadgets (src/game/Gadgets.hpp): what killed enemies drop, the repair
// kit, and the Force that flies around the ship and fires with it.

#include "TestGame.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Health.hpp"
#include "game/Components/Motion.hpp"
#include "game/Events/DespawnEvent.hpp"
#include "game/Gadgets.hpp"
#include "game/Ship.hpp"
#include "network/Protocol/Protocol.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/Gadgets.hpp"
#include "server/Components/NetEntity.hpp"
#include "server/Singleton.hpp"
#include "server/Spawn.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <set>

using namespace network::protocol;
using game::force::Slot;
using testing_game::enemyMissile;
using testing_game::Game;

namespace {

/// A game with player 0's ship, and helpers to hand it gadgets.
class GadgetGame {
public:
    GadgetGame()
        : player(game.join(0))
    {
    }

    ecs::Entity ship()
    {
        return game.shipOf(0);
    }

    game::Position shipAt()
    {
        return game.world.getComponent<game::Position>(ship());
    }

    /// Drops a pickup on the ship and lets it take it.
    void take(server::PickupKind kind)
    {
        (void)server::spawnPickup(game.world, kind, shipAt());
        game.steps(1);
    }

    /// Player 0's Force, or the null entity.
    ecs::Entity force()
    {
        const auto forces = game.all<server::Force>();
        return forces.empty() ? ecs::NullEntity : forces.front();
    }

    Slot slot()
    {
        return game.world.getComponent<server::Force>(force()).slot;
    }

    /// One press of POD: held for a step, then released for a step.
    void pressPod()
    {
        game.press(player, input::Pod);
        game.steps(1);
        game.press(player, 0);
        game.steps(1);
    }

    /// Kills `enemy` as a missile would.
    void kill(ecs::Entity enemy)
    {
        game.world.events().emit(game::DespawnRequest{
            .entity = enemy, .reason = game::DespawnReason::Killed, .killer = ecs::NullEntity});
        game.steps(1);
    }

    Game game;
    ecs::Entity player;
};

} // namespace

TEST(Gadgets, RepairKitHealsTheShipFully)
{
    GadgetGame play;
    play.game.world.getComponent<game::Health>(play.ship()).current = 1;

    play.take(server::PickupKind::Repair);

    EXPECT_EQ(
        play.game.world.getComponent<game::Health>(play.ship()).current, game::ship::MaxHealth);
    EXPECT_TRUE(play.game.ofKind(EntityKind::RepairKit).empty()); // Used up
}

TEST(Gadgets, PickupsDriftLeft)
{
    GadgetGame play;
    const ecs::Entity kit
        = server::spawnPickup(play.game.world, server::PickupKind::Repair, {.x = 1500, .y = 100});

    play.game.seconds(1.0F);

    EXPECT_NEAR(
        play.game.world.getComponent<game::Position>(kit).x, 1500.0F - game::pickup::Speed, 5.0F);
}

TEST(Gadgets, ForceCapsuleGivesAForceInFrontOfTheShip)
{
    GadgetGame play;

    play.take(server::PickupKind::Force);
    play.game.steps(1);

    const ecs::Entity force = play.force();
    ASSERT_NE(force, ecs::NullEntity);
    const auto& net = play.game.world.getComponent<server::NetEntity>(force);
    EXPECT_EQ(net.kind, EntityKind::Force);
    EXPECT_EQ(net.owner, 0);
    EXPECT_TRUE(play.game.ofKind(EntityKind::ForceCapsule).empty());
    const auto at = play.game.world.getComponent<game::Position>(force);
    EXPECT_FLOAT_EQ(at.x, play.shipAt().x + game::force::GapX);
    EXPECT_FLOAT_EQ(at.y, play.shipAt().y);
}

TEST(Gadgets, ASecondForceCapsuleGivesPoints)
{
    GadgetGame play;
    play.take(server::PickupKind::Force);
    const auto before = server::singleton<server::TeamScore>(play.game.world).value;

    play.take(server::PickupKind::Force);

    EXPECT_EQ(play.game.all<server::Force>().size(), 1U);
    EXPECT_EQ(server::singleton<server::TeamScore>(play.game.world).value,
        before + game::pickup::ForceBonusPoints);
}

TEST(Gadgets, TheForceFollowsTheShip)
{
    GadgetGame play;
    play.take(server::PickupKind::Force);

    play.game.press(play.player, input::Down);
    play.game.seconds(0.5F);

    const auto at = play.game.world.getComponent<game::Position>(play.force());
    EXPECT_FLOAT_EQ(at.x, play.shipAt().x + game::force::GapX);
    EXPECT_FLOAT_EQ(at.y, play.shipAt().y);
    // Same velocity as the ship: the clients interpolate both alike
    EXPECT_FLOAT_EQ(play.game.world.getComponent<game::Velocity>(play.force()).dy,
        play.game.world.getComponent<game::Velocity>(play.ship()).dy);
}

TEST(Gadgets, PodMovesTheForceAroundTheShip)
{
    GadgetGame play;
    play.take(server::PickupKind::Force);
    play.game.steps(1);
    ASSERT_EQ(play.slot(), Slot::Front);

    play.pressPod();
    EXPECT_EQ(play.slot(), Slot::Above);
    auto at = play.game.world.getComponent<game::Position>(play.force());
    EXPECT_FLOAT_EQ(at.x, play.shipAt().x);
    EXPECT_FLOAT_EQ(at.y, play.shipAt().y - game::force::GapY);

    play.pressPod();
    EXPECT_EQ(play.slot(), Slot::Behind);
    at = play.game.world.getComponent<game::Position>(play.force());
    EXPECT_FLOAT_EQ(at.x, play.shipAt().x - game::force::GapX);

    play.pressPod();
    EXPECT_EQ(play.slot(), Slot::Below);
    play.pressPod();
    EXPECT_EQ(play.slot(), Slot::Front); // Around again
}

TEST(Gadgets, HoldingPodMovesTheForceOnce)
{
    GadgetGame play;
    play.take(server::PickupKind::Force);
    play.game.steps(1);

    play.game.press(play.player, input::Pod);
    play.game.seconds(1.0F);

    EXPECT_EQ(play.slot(), Slot::Above);
}

TEST(Gadgets, TheForceFiresWithTheShipAwayFromIt)
{
    GadgetGame play;
    play.take(server::PickupKind::Force);
    play.game.steps(1);
    play.pressPod();
    play.pressPod(); // Behind the ship
    const auto missilesBefore = play.game.ofKind(EntityKind::PlayerMissile).size();

    play.game.press(play.player, input::Fire);
    play.game.steps(2);

    std::size_t backwards = 0;
    for (const ecs::Entity missile : play.game.ofKind(EntityKind::PlayerMissile)) {
        if (play.game.world.getComponent<game::Velocity>(missile).dx < 0.0F) {
            ++backwards;
            EXPECT_EQ(play.game.world.getComponent<server::NetEntity>(missile).owner, 0);
        }
    }
    EXPECT_EQ(backwards, 1U); // The Force's
    EXPECT_EQ(
        play.game.ofKind(EntityKind::PlayerMissile).size(), missilesBefore + 2); // And the ship's
}

TEST(Gadgets, TheForceStopsEnemyMissiles)
{
    GadgetGame play;
    play.take(server::PickupKind::Force);
    play.game.steps(1);
    const auto at = play.game.world.getComponent<game::Position>(play.force());

    (void)enemyMissile(play.game.world, at);
    play.game.steps(1);

    EXPECT_TRUE(play.game.ofKind(EntityKind::EnemyMissile).empty());
    EXPECT_EQ(
        play.game.world.getComponent<game::Health>(play.ship()).current, game::ship::MaxHealth);
    EXPECT_NE(play.force(), ecs::NullEntity); // Unharmed
}

TEST(Gadgets, TheForceIsLostWithTheShip)
{
    GadgetGame play;
    play.take(server::PickupKind::Force);
    ASSERT_NE(play.force(), ecs::NullEntity);

    play.kill(play.ship());
    play.game.steps(2);

    EXPECT_EQ(play.force(), ecs::NullEntity);
    EXPECT_TRUE(play.game.ofKind(EntityKind::Force).empty());
}

TEST(Gadgets, TheBossDropsARepairKitAndAForceCapsule)
{
    GadgetGame play;
    const ecs::Entity boss = server::spawnBoss(play.game.world, 400.0F, 10);

    play.kill(boss);

    EXPECT_EQ(play.game.ofKind(EntityKind::RepairKit).size(), 1U);
    EXPECT_EQ(play.game.ofKind(EntityKind::ForceCapsule).size(), 1U);
}

TEST(Gadgets, EnemiesSometimesDropSomething)
{
    GadgetGame play;
    std::set<std::uint16_t> repairs;
    std::set<std::uint16_t> capsules;
    constexpr int kills = 1000;

    for (int i = 0; i < kills; ++i) {
        play.kill(server::spawnPataPata(play.game.world, 300.0F, 0.0F));
        for (const ecs::Entity pickup : play.game.all<server::Pickup>()) {
            const auto& net = play.game.world.getComponent<server::NetEntity>(pickup);
            (net.kind == EntityKind::RepairKit ? repairs : capsules).insert(net.netId);
        }
    }

    // About 8 % and 5 %, with room for luck
    EXPECT_GT(repairs.size(), 40U);
    EXPECT_LT(repairs.size(), 130U);
    EXPECT_GT(capsules.size(), 20U);
    EXPECT_LT(capsules.size(), 90U);
}

TEST(Gadgets, EnemiesLeavingTheArenaDropNothing)
{
    GadgetGame play;

    for (int i = 0; i < 100; ++i) {
        const ecs::Entity drone = server::spawnPataPata(play.game.world, 300.0F, 0.0F);
        play.game.world.events().emit(game::DespawnRequest{.entity = drone,
            .reason = game::DespawnReason::OutOfBounds,
            .killer = ecs::NullEntity});
        play.game.steps(1);
    }

    EXPECT_TRUE(play.game.all<server::Pickup>().empty());
}
