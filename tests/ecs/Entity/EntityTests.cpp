#include "ecs/Entity/Entity.hpp"

#include <gtest/gtest.h>

#include <unordered_set>

using ecs::Entity;

TEST(Entity, DefaultConstructedIsNull)
{
    constexpr Entity entity;

    static_assert(!entity.valid());
    EXPECT_EQ(entity.id(), Entity::NullId);
    EXPECT_EQ(entity, ecs::NullEntity);
}

TEST(Entity, StoresIdAndGeneration)
{
    constexpr Entity entity{42, 3};

    static_assert(entity.valid());
    EXPECT_EQ(entity.id(), 42U);
    EXPECT_EQ(entity.generation(), 3U);
}

TEST(Entity, EqualityComparesIdAndGeneration)
{
    EXPECT_EQ((Entity{1, 0}), (Entity{1, 0}));
    EXPECT_NE((Entity{1, 0}), (Entity{2, 0}));
    EXPECT_NE((Entity{1, 0}), (Entity{1, 1}));
}

TEST(Entity, IsHashable)
{
    const std::unordered_set<Entity> entities{Entity{1, 0}, Entity{1, 1}, Entity{2, 0}};

    EXPECT_EQ(entities.size(), 3U);
    EXPECT_TRUE(entities.contains(Entity(1, 1)));
    EXPECT_FALSE(entities.contains(Entity(3, 0)));
}
