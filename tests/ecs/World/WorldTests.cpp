#include "ecs/World/World.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

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

} // namespace

TEST(World, CreatedEntityIsAliveAndEmpty)
{
    World world;

    const Entity entity = world.createEntity();

    EXPECT_TRUE(world.isAlive(entity));
    EXPECT_EQ(world.entityCount(), 1U);
    EXPECT_FALSE(world.hasComponent<Position>(entity));
}

TEST(World, ComponentsCanBeAddedReadAndRemoved)
{
    World world;
    const Entity entity = world.createEntity();

    world.addComponent(entity, Health{.current = 10});
    world.getComponent<Health>(entity).current -= 3;

    EXPECT_TRUE(world.hasComponent<Health>(entity));
    EXPECT_EQ(world.getComponent<Health>(entity).current, 7);

    EXPECT_TRUE(world.removeComponent<Health>(entity));
    EXPECT_FALSE(world.hasComponent<Health>(entity));
    EXPECT_EQ(world.tryGetComponent<Health>(entity), nullptr);
}

TEST(World, DestroyingAnEntityRemovesItsComponents)
{
    World world;
    const Entity destroyed = world.createEntity();
    world.addComponent(destroyed, Position{});
    world.addComponent(destroyed, Health{});

    EXPECT_TRUE(world.destroyEntity(destroyed));

    EXPECT_FALSE(world.isAlive(destroyed));
    EXPECT_FALSE(world.hasComponent<Position>(destroyed));
    EXPECT_FALSE(world.hasComponent<Health>(destroyed));
    EXPECT_EQ(world.entityCount(), 0U);
}

TEST(World, RecycledEntityStartsWithoutComponents)
{
    World world;
    const Entity old = world.createEntity();
    world.addComponent(old, Health{.current = 99});
    world.destroyEntity(old);

    const Entity recycled = world.createEntity();

    EXPECT_EQ(recycled.id(), old.id());
    EXPECT_FALSE(world.hasComponent<Health>(recycled));
}

TEST(World, DestroyingTwiceFails)
{
    World world;
    const Entity entity = world.createEntity();

    EXPECT_TRUE(world.destroyEntity(entity));
    EXPECT_FALSE(world.destroyEntity(entity));
}

TEST(World, AddingAComponentToADeadEntityThrows)
{
    World world;
    const Entity entity = world.createEntity();
    world.destroyEntity(entity);

    EXPECT_THROW(world.addComponent(entity, Health{}), std::invalid_argument);
    EXPECT_THROW(world.addComponent(ecs::NullEntity, Health{}), std::invalid_argument);
}

TEST(World, StaleHandleCannotReachTheNewEntityComponents)
{
    World world;
    const Entity stale = world.createEntity();
    world.destroyEntity(stale);
    const Entity recycled = world.createEntity();
    world.addComponent(recycled, Health{.current = 5});

    EXPECT_FALSE(world.hasComponent<Health>(stale));
    EXPECT_THROW((void)world.getComponent<Health>(stale), std::out_of_range);
    EXPECT_FALSE(world.removeComponent<Health>(stale));
    EXPECT_TRUE(world.hasComponent<Health>(recycled));
}

TEST(World, ClearDestroysEverything)
{
    World world;
    const Entity first = world.createEntity();
    const Entity second = world.createEntity();
    world.addComponent(first, Health{});

    world.clear();

    EXPECT_FALSE(world.isAlive(first));
    EXPECT_FALSE(world.isAlive(second));
    EXPECT_FALSE(world.hasComponent<Health>(first));
    EXPECT_EQ(world.entityCount(), 0U);
}

TEST(World, ConstAccess)
{
    World world;
    const Entity entity = world.createEntity();
    world.addComponent(entity, Health{.current = 4});

    const World& constWorld = world;

    EXPECT_EQ(constWorld.getComponent<Health>(entity).current, 4);
    EXPECT_NE(constWorld.tryGetComponent<Health>(entity), nullptr);
    EXPECT_EQ(constWorld.tryGetComponent<Position>(entity), nullptr);
}
