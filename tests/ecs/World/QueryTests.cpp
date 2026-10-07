#include "ecs/World/World.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

using ecs::Entity;
using ecs::World;

namespace {

struct Position {
    float x{};
    float y{};
};

struct Velocity {
    float dx{};
    float dy{};
};

struct PlayerTag { };

struct EnemyTag { };

/// Runs a query and returns the matching entities, sorted by id.
template <typename... Ts>
std::vector<Entity::Id> matchingIds(World& world)
{
    std::vector<Entity::Id> ids;
    world.query<Ts...>([&](Entity entity, Ts&...) { ids.push_back(entity.id()); });
    std::ranges::sort(ids);
    return ids;
}

} // namespace

TEST(Query, VisitsOnlyEntitiesOwningEveryComponent)
{
    World world;
    const Entity moving = world.createEntity(); // Position + Velocity
    const Entity still = world.createEntity(); // Position only
    const Entity ghost = world.createEntity(); // Velocity only
    world.addComponent(moving, Position{});
    world.addComponent(moving, Velocity{});
    world.addComponent(still, Position{});
    world.addComponent(ghost, Velocity{});

    EXPECT_EQ((matchingIds<Position, Velocity>(world)), std::vector<Entity::Id>{moving.id()});
    EXPECT_EQ(matchingIds<Position>(world), (std::vector<Entity::Id>{moving.id(), still.id()}));
}

TEST(Query, GivesWritableReferencesToComponents)
{
    World world;
    const Entity entity = world.createEntity();
    world.addComponent(entity, Position{.x = 0.0F, .y = 0.0F});
    world.addComponent(entity, Velocity{.dx = 2.0F, .dy = -1.0F});

    world.query<Position, Velocity>([](Entity, Position& position, Velocity& velocity) {
        position.x += velocity.dx;
        position.y += velocity.dy;
    });

    EXPECT_FLOAT_EQ(world.getComponent<Position>(entity).x, 2.0F);
    EXPECT_FLOAT_EQ(world.getComponent<Position>(entity).y, -1.0F);
}

TEST(Query, PassesTheMatchingComponentsOfEachEntity)
{
    World world;
    for (int index = 0; index < 5; ++index) {
        const Entity entity = world.createEntity();
        world.addComponent(entity, Position{.x = static_cast<float>(entity.id()), .y = 0.0F});
        world.addComponent(entity, Velocity{.dx = static_cast<float>(entity.id()), .dy = 0.0F});
    }

    int visited = 0;
    world.query<Position, Velocity>([&](Entity entity, Position& position, Velocity& velocity) {
        EXPECT_FLOAT_EQ(position.x, static_cast<float>(entity.id()));
        EXPECT_FLOAT_EQ(velocity.dx, static_cast<float>(entity.id()));
        ++visited;
    });

    EXPECT_EQ(visited, 5);
}

TEST(Query, FiltersWithTags)
{
    World world;
    const Entity player = world.createEntity();
    const Entity enemy = world.createEntity();
    for (const Entity entity : {player, enemy}) {
        world.addComponent(entity, Position{});
    }
    world.addComponent(player, PlayerTag{});
    world.addComponent(enemy, EnemyTag{});

    EXPECT_EQ((matchingIds<Position, PlayerTag>(world)), std::vector<Entity::Id>{player.id()});
    EXPECT_EQ((matchingIds<Position, EnemyTag>(world)), std::vector<Entity::Id>{enemy.id()});
}

TEST(Query, ComponentTypeNeverAddedMatchesNothing)
{
    World world;
    const Entity entity = world.createEntity();
    world.addComponent(entity, Position{});

    EXPECT_TRUE((matchingIds<Position, Velocity>(world)).empty());
}

TEST(Query, DestroyedEntitiesAreNotVisited)
{
    World world;
    const Entity kept = world.createEntity();
    const Entity destroyed = world.createEntity();
    world.addComponent(kept, Position{});
    world.addComponent(destroyed, Position{});

    world.destroyEntity(destroyed);

    EXPECT_EQ(matchingIds<Position>(world), std::vector<Entity::Id>{kept.id()});
}

TEST(Query, ResultDoesNotDependOnTheOrderOfComponentTypes)
{
    World world;
    for (int index = 0; index < 10; ++index) {
        const Entity entity = world.createEntity();
        world.addComponent(entity, Position{});
        if (index % 3 == 0) {
            world.addComponent(entity, Velocity{});
        }
    }

    EXPECT_EQ((matchingIds<Position, Velocity>(world)), (matchingIds<Velocity, Position>(world)));
    EXPECT_EQ((matchingIds<Position, Velocity>(world)).size(), 4U);
}
