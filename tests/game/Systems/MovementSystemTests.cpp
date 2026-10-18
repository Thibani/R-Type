#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "game/Systems/MovementSystem.hpp"

#include <gtest/gtest.h>

using game::MovementSystem;
using game::Position;
using game::Velocity;

TEST(MovementSystem, MovesByVelocityTimesDelta)
{
    ecs::World world;
    const ecs::Entity entity = world.createEntity();
    world.addComponent(entity, Position{.x = 10.0F, .y = 20.0F});
    world.addComponent(entity, Velocity{.dx = 100.0F, .dy = -50.0F});
    MovementSystem movement;

    movement.update(world, ecs::Time{0.5F, 0.5F});

    EXPECT_FLOAT_EQ(world.getComponent<Position>(entity).x, 60.0F);
    EXPECT_FLOAT_EQ(world.getComponent<Position>(entity).y, -5.0F);
}

TEST(MovementSystem, SameDistanceWhateverTheNumberOfSteps)
{
    // One 0.5 s step or four 0.125 s steps: same place (exact in float)
    ecs::World world;
    const ecs::Entity once = world.createEntity();
    world.addComponent(once, Position{});
    world.addComponent(once, Velocity{.dx = 64.0F, .dy = 0.0F});
    ecs::World otherWorld;
    const ecs::Entity often = otherWorld.createEntity();
    otherWorld.addComponent(often, Position{});
    otherWorld.addComponent(often, Velocity{.dx = 64.0F, .dy = 0.0F});
    MovementSystem movement;

    movement.update(world, ecs::Time{0.5F, 0.5F});
    for (int step = 0; step < 4; ++step) {
        movement.update(otherWorld, ecs::Time{0.125F, 0.0F});
    }

    EXPECT_FLOAT_EQ(
        world.getComponent<Position>(once).x, otherWorld.getComponent<Position>(often).x);
}

TEST(MovementSystem, EntityWithoutVelocityStaysPut)
{
    ecs::World world;
    const ecs::Entity scenery = world.createEntity();
    world.addComponent(scenery, Position{.x = 5.0F, .y = 5.0F});
    MovementSystem movement;

    movement.update(world, ecs::Time{1.0F, 1.0F});

    EXPECT_FLOAT_EQ(world.getComponent<Position>(scenery).x, 5.0F);
}
