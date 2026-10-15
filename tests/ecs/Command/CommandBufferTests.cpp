#include "ecs/Command/CommandBuffer.hpp"
#include "ecs/World/World.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using ecs::CommandBuffer;
using ecs::Entity;
using ecs::World;

namespace {

struct Position {
    float x{};
    float y{};
};

struct Health {
    int current{};
};

struct MissileTag { };

/// A component that cannot be copied, only moved.
struct Script {
    std::unique_ptr<int> data;
};

} // namespace

TEST(CommandBuffer, NothingHappensBeforeApply)
{
    World world;
    CommandBuffer commands;
    const Entity entity = world.createEntity();

    commands.destroyEntity(entity);
    (void)commands.createEntity();

    EXPECT_TRUE(world.isAlive(entity));
    EXPECT_EQ(world.entityCount(), 1U);
    EXPECT_EQ(commands.size(), 2U);
}

TEST(CommandBuffer, ApplyRunsAndForgetsTheCommands)
{
    World world;
    CommandBuffer commands;
    const Entity entity = world.createEntity();
    commands.destroyEntity(entity);

    commands.apply(world);

    EXPECT_FALSE(world.isAlive(entity));
    EXPECT_TRUE(commands.empty());
}

TEST(CommandBuffer, CreatesEntitiesWithTheirComponents)
{
    World world;
    CommandBuffer commands;

    commands.createEntity().with(Position{.x = 1.0F, .y = 2.0F}).with(MissileTag{});
    commands.apply(world);

    ASSERT_EQ(world.entityCount(), 1U);
    int matches = 0;
    world.query<Position, MissileTag>([&](Entity, Position& position, MissileTag&) {
        EXPECT_FLOAT_EQ(position.x, 1.0F);
        ++matches;
    });
    EXPECT_EQ(matches, 1);
}

TEST(CommandBuffer, ThenReceivesTheCreatedEntity)
{
    World world;
    CommandBuffer commands;
    Entity created;

    commands.createEntity().with(Health{.current = 3}).then([&](World& target, Entity entity) {
        created = entity;
        EXPECT_TRUE(target.hasComponent<Health>(entity)); // Components are already there
    });
    commands.apply(world);

    EXPECT_TRUE(world.isAlive(created));
}

TEST(CommandBuffer, AddsAndRemovesComponents)
{
    World world;
    CommandBuffer commands;
    const Entity entity = world.createEntity();
    world.addComponent(entity, Position{});

    commands.addComponent(entity, Health{.current = 5});
    commands.removeComponent<Position>(entity);
    commands.apply(world);

    EXPECT_EQ(world.getComponent<Health>(entity).current, 5);
    EXPECT_FALSE(world.hasComponent<Position>(entity));
}

TEST(CommandBuffer, CommandsRunInRecordingOrder)
{
    World world;
    CommandBuffer commands;
    const Entity entity = world.createEntity();

    commands.addComponent(entity, Health{.current = 1});
    commands.addComponent(entity, Health{.current = 2}); // Replaces the first one
    commands.apply(world);

    EXPECT_EQ(world.getComponent<Health>(entity).current, 2);
}

TEST(CommandBuffer, CommandsOnDeadEntitiesAreSkipped)
{
    World world;
    CommandBuffer commands;
    const Entity entity = world.createEntity();

    commands.destroyEntity(entity);
    commands.addComponent(entity, Health{}); // The entity is dead by then
    commands.removeComponent<Health>(entity);
    commands.destroyEntity(entity); // Destroying twice is harmless

    EXPECT_NO_THROW(commands.apply(world));
    EXPECT_EQ(world.entityCount(), 0U);
}

TEST(CommandBuffer, StaleHandlesDoNotAffectTheNewEntity)
{
    World world;
    CommandBuffer commands;
    const Entity stale = world.createEntity();
    world.destroyEntity(stale);
    const Entity recycled = world.createEntity(); // Same id, new generation

    commands.destroyEntity(stale);
    commands.addComponent(stale, Health{});
    commands.apply(world);

    EXPECT_TRUE(world.isAlive(recycled));
    EXPECT_FALSE(world.hasComponent<Health>(recycled));
}

TEST(CommandBuffer, RunRecordsAnyAction)
{
    World world;
    CommandBuffer commands;
    std::vector<std::string> log;

    commands.run([&](World&) { log.emplace_back("first"); });
    commands.run([&](World&) { log.emplace_back("second"); });
    commands.apply(world);

    EXPECT_EQ(log, (std::vector<std::string>{"first", "second"}));
}

TEST(CommandBuffer, CommandsRecordedWhileApplyingAreAppliedToo)
{
    World world;
    CommandBuffer commands;

    commands.run([&](World&) {
        commands.createEntity().with(Health{.current = 1}); // Recorded during apply
    });
    commands.apply(world);

    EXPECT_EQ(world.entityCount(), 1U);
    EXPECT_TRUE(commands.empty());
}

TEST(CommandBuffer, ApplyIsNotReentrant)
{
    World world;
    CommandBuffer commands;
    int runs = 0;

    commands.run([&](World& target) {
        ++runs;
        commands.apply(target); // Ignored
    });
    commands.apply(world);

    EXPECT_EQ(runs, 1);
}

TEST(CommandBuffer, ClearForgetsWithoutRunning)
{
    World world;
    CommandBuffer commands;
    const Entity entity = world.createEntity();
    commands.destroyEntity(entity);

    commands.clear();
    commands.apply(world);

    EXPECT_TRUE(world.isAlive(entity));
}

TEST(CommandBuffer, AcceptsMoveOnlyComponents)
{
    World world;
    CommandBuffer commands;
    const Entity entity = world.createEntity();

    commands.addComponent(entity, Script{.data = std::make_unique<int>(42)});
    commands.createEntity().with(Script{.data = std::make_unique<int>(7)});
    commands.apply(world);

    EXPECT_EQ(*world.getComponent<Script>(entity).data, 42);
    EXPECT_EQ(world.entityCount(), 2U);
}
