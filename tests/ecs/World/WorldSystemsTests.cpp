#include "ecs/System/System.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"

#include <gtest/gtest.h>

using ecs::Entity;
using ecs::Time;
using ecs::World;

namespace {

struct Position {
    float x{};
    float y{};
};

struct Velocity {
    float dx{}; // Units per second
    float dy{};
};

/// A minimal movement system, as game code would write it.
class MovementSystem final : public ecs::System {
public:
    void update(World& world, const Time& time) override
    {
        world.query<Position, Velocity>([&](Entity, Position& position, Velocity& velocity) {
            position.x += velocity.dx * time.deltaSeconds();
            position.y += velocity.dy * time.deltaSeconds();
        });
    }
};

} // namespace

TEST(WorldSystems, UpdateRunsTheRegisteredSystems)
{
    World world;
    world.systems().add<MovementSystem>();
    const Entity ship = world.createEntity();
    world.addComponent(ship, Position{.x = 0.0F, .y = 0.0F});
    world.addComponent(ship, Velocity{.dx = 10.0F, .dy = -4.0F});

    world.update(Time{0.5F, 0.5F});

    EXPECT_FLOAT_EQ(world.getComponent<Position>(ship).x, 5.0F);
    EXPECT_FLOAT_EQ(world.getComponent<Position>(ship).y, -2.0F);
}

TEST(WorldSystems, MovementDoesNotDependOnTheFrameRate)
{
    World slow; // 1 frame of 1 second
    World fast; // 4 frames of 0.25 second
    for (World* world : {&slow, &fast}) {
        world->systems().add<MovementSystem>();
        const Entity ship = world->createEntity();
        world->addComponent(ship, Position{});
        world->addComponent(ship, Velocity{.dx = 100.0F, .dy = 0.0F});
    }

    Time time;
    slow.update(time.advanced(1.0F));
    for (int frame = 0; frame < 4; ++frame) {
        time = time.advanced(0.25F);
        fast.update(time);
    }

    const Entity ship{0, 0};
    EXPECT_FLOAT_EQ(slow.getComponent<Position>(ship).x, 100.0F);
    EXPECT_FLOAT_EQ(fast.getComponent<Position>(ship).x, 100.0F);
}

TEST(WorldSystems, ClearKeepsTheSystems)
{
    World world;
    world.systems().add<MovementSystem>();
    (void)world.createEntity();

    world.clear();

    EXPECT_EQ(world.entityCount(), 0U);
    EXPECT_TRUE(world.systems().has<MovementSystem>());
}
