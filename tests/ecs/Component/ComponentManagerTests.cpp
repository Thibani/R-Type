#include "ecs/Component/ComponentManager.hpp"
#include "ecs/Entity/EntityManager.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

using ecs::ComponentManager;
using ecs::Entity;
using ecs::EntityManager;

namespace {

struct Position {
    float x{};
    float y{};
};

struct Velocity {
    float dx{};
    float dy{};
};

struct EnemyTag { };

} // namespace

TEST(ComponentManager, StoresDifferentComponentTypesIndependently)
{
    EntityManager entities;
    ComponentManager components;
    const Entity entity = entities.create();

    components.add(entity, Position{.x = 1.0F, .y = 2.0F});
    components.add(entity, Velocity{.dx = 3.0F, .dy = 4.0F});

    EXPECT_FLOAT_EQ(components.get<Position>(entity).x, 1.0F);
    EXPECT_FLOAT_EQ(components.get<Velocity>(entity).dx, 3.0F);
}

TEST(ComponentManager, HasReflectsAddAndRemove)
{
    EntityManager entities;
    ComponentManager components;
    const Entity entity = entities.create();

    EXPECT_FALSE(components.has<Position>(entity));

    components.add(entity, Position{});
    EXPECT_TRUE(components.has<Position>(entity));
    EXPECT_FALSE(components.has<Velocity>(entity));

    EXPECT_TRUE(components.remove<Position>(entity));
    EXPECT_FALSE(components.has<Position>(entity));
}

TEST(ComponentManager, UnknownComponentTypeIsHandledSafely)
{
    ComponentManager components;
    const Entity entity{0, 0};

    EXPECT_FALSE(components.has<Velocity>(entity));
    EXPECT_FALSE(components.remove<Velocity>(entity));
    EXPECT_EQ(components.tryGet<Velocity>(entity), nullptr);
    EXPECT_THROW((void)components.get<Velocity>(entity), std::out_of_range);

    // Reading must never create a storage as a side effect.
    EXPECT_EQ(components.findStorage<Velocity>(), nullptr);
}

TEST(ComponentManager, ConstAccess)
{
    ComponentManager components;
    const Entity entity{0, 0};
    components.add(entity, Position{.x = 5.0F, .y = 0.0F});

    const ComponentManager& constComponents = components;

    EXPECT_FLOAT_EQ(constComponents.get<Position>(entity).x, 5.0F);
    EXPECT_NE(constComponents.tryGet<Position>(entity), nullptr);
    EXPECT_THROW((void)constComponents.get<Velocity>(entity), std::out_of_range);
}

TEST(ComponentManager, TagsAreRegularComponents)
{
    ComponentManager components;
    const Entity enemy{0, 0};
    const Entity player{1, 0};

    components.add(enemy, EnemyTag{});

    EXPECT_TRUE(components.has<EnemyTag>(enemy));
    EXPECT_FALSE(components.has<EnemyTag>(player));
}

TEST(ComponentManager, RemoveAllDetachesEveryComponentOfAnEntity)
{
    ComponentManager components;
    const Entity removed{0, 0};
    const Entity kept{1, 0};
    components.add(removed, Position{});
    components.add(removed, Velocity{});
    components.add(kept, Position{});

    components.removeAll(removed);

    EXPECT_FALSE(components.has<Position>(removed));
    EXPECT_FALSE(components.has<Velocity>(removed));
    EXPECT_TRUE(components.has<Position>(kept));
}

TEST(ComponentManager, ClearRemovesEveryComponent)
{
    ComponentManager components;
    components.add(Entity{0, 0}, Position{});
    components.add(Entity{1, 0}, Velocity{});

    components.clear();

    EXPECT_FALSE(components.has<Position>(Entity{0, 0}));
    EXPECT_FALSE(components.has<Velocity>(Entity{1, 0}));
}

TEST(ComponentManager, StorageGivesAccessToAllComponentsOfAType)
{
    ComponentManager components;
    components.add(Entity{0, 0}, Position{.x = 1.0F, .y = 0.0F});
    components.add(Entity{1, 0}, Position{.x = 2.0F, .y = 0.0F});

    float sum = 0.0F;
    for (const Position& position : components.storage<Position>().components()) {
        sum += position.x;
    }

    EXPECT_FLOAT_EQ(sum, 3.0F);
}
