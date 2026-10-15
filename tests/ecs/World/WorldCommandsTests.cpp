#include "ecs/Event/Subscription.hpp"
#include "ecs/System/System.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"

#include <gtest/gtest.h>

using ecs::Entity;
using ecs::Time;
using ecs::World;

namespace {

struct Health {
    int current{};
};

struct Velocity {
    float dx{};
};

struct DiedEvent {
    Entity entity;
};

/// Destroys dead entities FROM INSIDE a query, through the command buffer.
class CleanupSystem final : public ecs::System {
public:
    void update(World& world, const Time& /*time*/) override
    {
        world.query<Health>([&](Entity entity, Health& health) {
            if (health.current <= 0) {
                world.commands().destroyEntity(entity);
                world.events().emit(DiedEvent{.entity = entity});
            }
        });
    }
};

/// Spawns a "debris" entity for every death, from an event handler.
class DebrisSystem final : public ecs::System {
public:
    explicit DebrisSystem(World& world)
        : _onDied(world.events().subscribe<DiedEvent>([&world](const DiedEvent&) {
            world.commands().createEntity().with(Velocity{.dx = -10.0F});
        }))
    {
    }

    void update(World& /*world*/, const Time& /*time*/) override { }

private:
    ecs::Subscription _onDied;
};

} // namespace

TEST(WorldCommands, EntitiesCanBeDestroyedFromInsideAQuery)
{
    World world;
    world.systems().add<CleanupSystem>();
    for (int index = 0; index < 10; ++index) {
        const Entity entity = world.createEntity();
        world.addComponent(entity, Health{.current = index % 2}); // Half of them are dead
    }

    world.update(Time{});

    EXPECT_EQ(world.entityCount(), 5U);
    world.query<Health>([](Entity, Health& health) { EXPECT_EQ(health.current, 1); });
}

TEST(WorldCommands, CommandsRecordedByEventHandlersAreAppliedInTheSameUpdate)
{
    World world;
    world.systems().add<CleanupSystem>();
    world.systems().add<DebrisSystem>(world);
    const Entity dead = world.createEntity();
    world.addComponent(dead, Health{.current = 0});

    world.update(Time{});

    // update(): CleanupSystem records the destruction and emits DiedEvent,
    // commands are applied, the event is delivered, DebrisSystem records a
    // creation, and commands are applied again.
    EXPECT_FALSE(world.isAlive(dead));
    int debris = 0;
    world.query<Velocity>([&](Entity, Velocity&) { ++debris; });
    EXPECT_EQ(debris, 1);
    EXPECT_TRUE(world.commands().empty());
}

TEST(WorldCommands, ClearDropsPendingCommands)
{
    World world;
    const Entity entity = world.createEntity();
    world.commands().addComponent(entity, Health{});

    world.clear();

    EXPECT_TRUE(world.commands().empty());
}
