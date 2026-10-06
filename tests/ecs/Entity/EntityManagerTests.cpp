#include "ecs/Entity/EntityManager.hpp"

#include <gtest/gtest.h>

using ecs::Entity;
using ecs::EntityManager;

TEST(EntityManager, CreatesAliveEntities)
{
    EntityManager manager;

    const Entity first = manager.create();
    const Entity second = manager.create();

    EXPECT_TRUE(first.valid());
    EXPECT_TRUE(manager.isAlive(first));
    EXPECT_TRUE(manager.isAlive(second));
    EXPECT_NE(first, second);
    EXPECT_EQ(manager.aliveCount(), 2U);
}

TEST(EntityManager, DestroyedEntityIsNoLongerAlive)
{
    EntityManager manager;
    const Entity entity = manager.create();

    EXPECT_TRUE(manager.destroy(entity));

    EXPECT_FALSE(manager.isAlive(entity));
    EXPECT_EQ(manager.aliveCount(), 0U);
}

TEST(EntityManager, DestroyingTwiceFails)
{
    EntityManager manager;
    const Entity entity = manager.create();

    EXPECT_TRUE(manager.destroy(entity));
    EXPECT_FALSE(manager.destroy(entity));
    EXPECT_EQ(manager.aliveCount(), 0U);
}

TEST(EntityManager, RecycledIdGetsNewGeneration)
{
    EntityManager manager;
    const Entity original = manager.create();
    manager.destroy(original);

    const Entity recycled = manager.create();

    EXPECT_EQ(recycled.id(), original.id());
    EXPECT_EQ(recycled.generation(), original.generation() + 1);
    EXPECT_TRUE(manager.isAlive(recycled));
}

TEST(EntityManager, StaleHandleIsRejected)
{
    EntityManager manager;
    const Entity stale = manager.create();
    manager.destroy(stale);
    const Entity recycled = manager.create();

    EXPECT_FALSE(manager.isAlive(stale));
    EXPECT_FALSE(manager.destroy(stale)); // Must not destroy the new entity
    EXPECT_TRUE(manager.isAlive(recycled));
}

TEST(EntityManager, NullAndUnknownHandlesAreNotAlive)
{
    EntityManager manager;

    EXPECT_FALSE(manager.isAlive(ecs::NullEntity));
    EXPECT_FALSE(manager.isAlive(Entity{1234, 0}));
    EXPECT_FALSE(manager.destroy(ecs::NullEntity));
}

TEST(EntityManager, ClearInvalidatesEveryHandle)
{
    EntityManager manager;
    const Entity first = manager.create();
    const Entity second = manager.create();

    manager.clear();

    EXPECT_FALSE(manager.isAlive(first));
    EXPECT_FALSE(manager.isAlive(second));
    EXPECT_EQ(manager.aliveCount(), 0U);

    const Entity next = manager.create();
    EXPECT_EQ(next.id(), 0U);
    EXPECT_TRUE(manager.isAlive(next));
}
