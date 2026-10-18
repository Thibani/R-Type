#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "game/Systems/MovementSystem.hpp"

#include <gtest/gtest.h>

using ecs::Entity;
using ecs::Time;
using ecs::World;
using game::Position;
using game::Velocity;

namespace {

Entity spawn(World& world, Position position, Velocity velocity)
{
    const Entity entity = world.createEntity();
    world.addComponent(entity, position);
    world.addComponent(entity, velocity);
    return entity;
}

/// A World running only the MovementSystem.
class MovementSystemTest : public ::testing::Test {
protected:
    MovementSystemTest()
    {
        _world.systems().add<game::MovementSystem>();
    }

    World _world;
};

} // namespace

TEST_F(MovementSystemTest, MovesByVelocityTimesDelta)
{
    const Entity ship = spawn(_world, {.x = 100.0F, .y = 200.0F}, {.dx = 300.0F, .dy = -60.0F});

    _world.update(Time{0.5F, 0.5F});

    EXPECT_FLOAT_EQ(_world.getComponent<Position>(ship).x, 250.0F);
    EXPECT_FLOAT_EQ(_world.getComponent<Position>(ship).y, 170.0F);
}

TEST_F(MovementSystemTest, DoesNotChangeTheVelocity)
{
    const Entity ship = spawn(_world, {}, {.dx = 10.0F, .dy = 20.0F});

    _world.update(Time{1.0F, 1.0F});

    EXPECT_FLOAT_EQ(_world.getComponent<Velocity>(ship).dx, 10.0F);
    EXPECT_FLOAT_EQ(_world.getComponent<Velocity>(ship).dy, 20.0F);
}

TEST_F(MovementSystemTest, ZeroVelocityOrZeroDeltaDoesNotMove)
{
    const Entity still = spawn(_world, {.x = 5.0F, .y = 5.0F}, {});
    const Entity moving = spawn(_world, {.x = 5.0F, .y = 5.0F}, {.dx = 100.0F, .dy = 100.0F});

    _world.update(Time{}); // First frame: delta = 0
    _world.update(Time{1.0F, 1.0F});

    EXPECT_FLOAT_EQ(_world.getComponent<Position>(still).x, 5.0F);
    EXPECT_FLOAT_EQ(_world.getComponent<Position>(moving).x, 105.0F);
}

TEST_F(MovementSystemTest, IgnoresEntitiesWithoutVelocity)
{
    const Entity scenery = _world.createEntity();
    _world.addComponent(scenery, Position{.x = 42.0F, .y = 7.0F});

    _world.update(Time{1.0F, 1.0F});

    EXPECT_FLOAT_EQ(_world.getComponent<Position>(scenery).x, 42.0F);
}

TEST(MovementSystem, DoesNotDependOnTheFrameRate)
{
    World slow; // 1 frame of 1 s
    World fast; // 60 frames of 1/60 s, like the server's fixed step
    slow.systems().add<game::MovementSystem>();
    fast.systems().add<game::MovementSystem>();
    const Entity slowShip = spawn(slow, {}, {.dx = 120.0F, .dy = 0.0F});
    const Entity fastShip = spawn(fast, {}, {.dx = 120.0F, .dy = 0.0F});

    slow.update(Time{}.advanced(1.0F));
    Time time;
    for (int frame = 0; frame < 60; ++frame) {
        time = time.advanced(1.0F / 60.0F);
        fast.update(time);
    }

    EXPECT_FLOAT_EQ(slow.getComponent<Position>(slowShip).x, 120.0F);
    EXPECT_NEAR(fast.getComponent<Position>(fastShip).x, 120.0F, 1e-3F);
}
