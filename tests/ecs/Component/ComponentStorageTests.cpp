#include "ecs/Component/ComponentStorage.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

using ecs::ComponentStorage;
using ecs::Entity;

namespace {

struct Health {
    int current{};
    int maximum{};
};

} // namespace

TEST(ComponentStorage, InsertedComponentCanBeRetrieved)
{
    ComponentStorage<Health> storage;
    const Entity entity{0, 0};

    storage.insert(entity, Health{.current = 80, .maximum = 100});

    EXPECT_TRUE(storage.contains(entity));
    EXPECT_EQ(storage.get(entity).current, 80);
    EXPECT_EQ(storage.get(entity).maximum, 100);
    EXPECT_EQ(storage.size(), 1U);
}

TEST(ComponentStorage, InsertReturnsAModifiableReference)
{
    ComponentStorage<Health> storage;
    const Entity entity{0, 0};

    Health& health = storage.insert(entity, Health{.current = 10, .maximum = 10});
    health.current = 3;

    EXPECT_EQ(storage.get(entity).current, 3);
}

TEST(ComponentStorage, InsertingTwiceReplacesTheComponent)
{
    ComponentStorage<Health> storage;
    const Entity entity{0, 0};

    storage.insert(entity, Health{.current = 1, .maximum = 1});
    storage.insert(entity, Health{.current = 2, .maximum = 2});

    EXPECT_EQ(storage.get(entity).current, 2);
    EXPECT_EQ(storage.size(), 1U);
}

TEST(ComponentStorage, MissingComponentIsReported)
{
    ComponentStorage<Health> storage;
    const Entity entity{5, 0};

    EXPECT_FALSE(storage.contains(entity));
    EXPECT_EQ(storage.tryGet(entity), nullptr);
    EXPECT_THROW((void)storage.get(entity), std::out_of_range);
    EXPECT_FALSE(storage.contains(ecs::NullEntity));
}

TEST(ComponentStorage, RemoveDetachesTheComponent)
{
    ComponentStorage<Health> storage;
    const Entity entity{0, 0};
    storage.insert(entity, Health{});

    EXPECT_TRUE(storage.remove(entity));

    EXPECT_FALSE(storage.contains(entity));
    EXPECT_EQ(storage.size(), 0U);
    EXPECT_FALSE(storage.remove(entity));
}

TEST(ComponentStorage, RemovingFromTheMiddleKeepsOtherComponentsIntact)
{
    ComponentStorage<Health> storage;
    const Entity first{0, 0};
    const Entity middle{1, 0};
    const Entity last{2, 0};
    storage.insert(first, Health{.current = 1, .maximum = 1});
    storage.insert(middle, Health{.current = 2, .maximum = 2});
    storage.insert(last, Health{.current = 3, .maximum = 3});

    storage.remove(middle); // The last component is moved into the hole

    EXPECT_EQ(storage.size(), 2U);
    EXPECT_EQ(storage.get(first).current, 1);
    EXPECT_EQ(storage.get(last).current, 3);
    EXPECT_FALSE(storage.contains(middle));
}

TEST(ComponentStorage, StaleHandleDoesNotMatchTheNewOwner)
{
    ComponentStorage<Health> storage;
    const Entity current{4, 1}; // Id 4 recycled: generation 1
    const Entity stale{4, 0};
    storage.insert(current, Health{.current = 50, .maximum = 50});

    EXPECT_FALSE(storage.contains(stale));
    EXPECT_EQ(storage.tryGet(stale), nullptr);
    EXPECT_FALSE(storage.remove(stale));
    EXPECT_TRUE(storage.contains(current));
}

TEST(ComponentStorage, RecycledIdOverwritesTheStaleComponent)
{
    ComponentStorage<Health> storage;
    const Entity stale{2, 0};
    const Entity recycled{2, 1};
    storage.insert(stale, Health{.current = 1, .maximum = 1});

    // The stale component was never removed, then the id is reused.
    storage.insert(recycled, Health{.current = 50, .maximum = 50});

    EXPECT_EQ(storage.size(), 1U); // No orphan left behind
    EXPECT_FALSE(storage.contains(stale));
    EXPECT_EQ(storage.get(recycled).current, 50);
}

TEST(ComponentStorage, DenseArraysStayAligned)
{
    ComponentStorage<Health> storage;
    storage.insert(Entity{7, 0}, Health{.current = 7, .maximum = 7});
    storage.insert(Entity{2, 0}, Health{.current = 2, .maximum = 2});

    const auto entities = storage.entities();
    const auto components = storage.components();

    ASSERT_EQ(entities.size(), components.size());
    for (std::size_t index = 0; index < entities.size(); ++index) {
        EXPECT_EQ(static_cast<int>(entities[index].id()), components[index].current);
    }
}

TEST(ComponentStorage, ClearRemovesEverything)
{
    ComponentStorage<Health> storage;
    storage.insert(Entity{0, 0}, Health{});
    storage.insert(Entity{1, 0}, Health{});

    storage.clear();

    EXPECT_EQ(storage.size(), 0U);
    EXPECT_FALSE(storage.contains(Entity{0, 0}));
}
