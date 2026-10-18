#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Components/Confinement.hpp"
#include "game/Components/Motion.hpp"
#include "game/Systems/ConfinementSystem.hpp"
#include "game/Systems/MovementSystem.hpp"

#include <gtest/gtest.h>

using ecs::Entity;
using ecs::Time;
using ecs::World;
using game::ConfinedToArena;
using game::Position;
using game::Velocity;

namespace {

constexpr ConfinedToArena ShipSize{.halfWidth = 16.0F, .halfHeight = 8.0F};

Entity spawnShip(World& world, Position position, Velocity velocity = {})
{
    const Entity ship = world.createEntity();
    world.addComponent(ship, position);
    world.addComponent(ship, velocity);
    world.addComponent(ship, ShipSize);
    return ship;
}

/// A World running movement then confinement, as the game does.
class ConfinementSystemTest : public ::testing::Test {
protected:
    ConfinementSystemTest()
    {
        _world.systems().add<game::MovementSystem>();
        _world.systems().add<game::ConfinementSystem>();
    }

    World _world;
};

} // namespace

TEST_F(ConfinementSystemTest, LeavesEntitiesInsideTheArenaUntouched)
{
    const Entity ship = spawnShip(_world, {.x = 500.0F, .y = 300.0F});

    _world.update(Time{1.0F, 1.0F});

    EXPECT_FLOAT_EQ(_world.getComponent<Position>(ship).x, 500.0F);
    EXPECT_FLOAT_EQ(_world.getComponent<Position>(ship).y, 300.0F);
}

TEST_F(ConfinementSystemTest, StopsAtEveryEdgeKeepingTheWholeShipVisible)
{
    const Entity left = spawnShip(_world, {.x = 20.0F, .y = 500.0F}, {.dx = -1000.0F, .dy = 0.0F});
    const Entity right
        = spawnShip(_world, {.x = 1900.0F, .y = 500.0F}, {.dx = 1000.0F, .dy = 0.0F});
    const Entity top = spawnShip(_world, {.x = 500.0F, .y = 10.0F}, {.dx = 0.0F, .dy = -1000.0F});
    const Entity bottom
        = spawnShip(_world, {.x = 500.0F, .y = 1070.0F}, {.dx = 0.0F, .dy = 1000.0F});

    _world.update(Time{1.0F, 1.0F});

    EXPECT_FLOAT_EQ(_world.getComponent<Position>(left).x, ShipSize.halfWidth);
    EXPECT_FLOAT_EQ(
        _world.getComponent<Position>(right).x, game::arena::Width - ShipSize.halfWidth);
    EXPECT_FLOAT_EQ(_world.getComponent<Position>(top).y, ShipSize.halfHeight);
    EXPECT_FLOAT_EQ(
        _world.getComponent<Position>(bottom).y, game::arena::Height - ShipSize.halfHeight);
}

TEST_F(ConfinementSystemTest, KeepsTheVelocity)
{
    const Entity ship = spawnShip(_world, {.x = 20.0F, .y = 500.0F}, {.dx = -1000.0F, .dy = 5.0F});

    _world.update(Time{1.0F, 1.0F});

    // Only the position is corrected: the controls decide the speed.
    EXPECT_FLOAT_EQ(_world.getComponent<Velocity>(ship).dx, -1000.0F);
}

TEST_F(ConfinementSystemTest, IgnoresEntitiesThatAreNotConfined)
{
    const Entity missile = _world.createEntity();
    _world.addComponent(missile, Position{.x = 1900.0F, .y = 500.0F});
    _world.addComponent(missile, Velocity{.dx = 1000.0F, .dy = 0.0F});

    _world.update(Time{1.0F, 1.0F});

    EXPECT_FLOAT_EQ(_world.getComponent<Position>(missile).x, 2900.0F); // Left the screen
}

TEST_F(ConfinementSystemTest, EntityBiggerThanTheArenaStaysCentred)
{
    const Entity giant = _world.createEntity();
    _world.addComponent(giant, Position{.x = 0.0F, .y = 0.0F});
    _world.addComponent(giant, ConfinedToArena{.halfWidth = 5000.0F, .halfHeight = 5000.0F});

    _world.update(Time{1.0F, 1.0F});

    EXPECT_FLOAT_EQ(_world.getComponent<Position>(giant).x, game::arena::Width / 2.0F);
    EXPECT_FLOAT_EQ(_world.getComponent<Position>(giant).y, game::arena::Height / 2.0F);
}
