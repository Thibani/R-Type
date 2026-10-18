#include "ecs/Event/Subscription.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Collision.hpp"
#include "game/Components/Motion.hpp"
#include "game/Events/CollisionEvent.hpp"
#include "game/Systems/CollisionSystem.hpp"

#include <gtest/gtest.h>

#include <utility>
#include <vector>

using ecs::Entity;
using ecs::Time;
using ecs::World;
using game::CollisionEvent;
using game::Hitbox;
using game::Position;
namespace layer = game::layer;

namespace {

constexpr Hitbox Ship{.halfWidth = 32.0F,
    .halfHeight = 16.0F,
    .layer = layer::PlayerShip,
    .collidesWith = layer::Enemy | layer::EnemyMissile};

constexpr Hitbox PlayerMissile{.halfWidth = 8.0F,
    .halfHeight = 2.0F,
    .layer = layer::PlayerMissile,
    .collidesWith = layer::Enemy};

constexpr Hitbox Bydo{.halfWidth = 24.0F,
    .halfHeight = 24.0F,
    .layer = layer::Enemy,
    .collidesWith = layer::PlayerShip | layer::PlayerMissile};

/// A World running the CollisionSystem, recording every collision event.
class CollisionSystemTest : public ::testing::Test {
protected:
    CollisionSystemTest()
        : _onCollision(_world.events().subscribe<CollisionEvent>(
              [this](const CollisionEvent& event) { _collisions.push_back(event); }))
    {
        _world.systems().add<game::CollisionSystem>();
    }

    Entity spawn(Position position, Hitbox hitbox)
    {
        const Entity entity = _world.createEntity();
        _world.addComponent(entity, position);
        _world.addComponent(entity, hitbox);
        return entity;
    }

    /// Runs one step and returns the pairs reported during it.
    std::vector<std::pair<Entity, Entity>> step()
    {
        _collisions.clear();
        _world.update(Time{1.0F / 60.0F, 0.0F});
        std::vector<std::pair<Entity, Entity>> pairs;
        pairs.reserve(_collisions.size());
        for (const CollisionEvent& event : _collisions) {
            pairs.emplace_back(event.first, event.second);
        }
        return pairs;
    }

    World _world;
    std::vector<CollisionEvent> _collisions;
    ecs::Subscription _onCollision;
};

using Pairs = std::vector<std::pair<Entity, Entity>>;

} // namespace

TEST_F(CollisionSystemTest, OverlappingBoxesCollide)
{
    const Entity missile = spawn({.x = 100.0F, .y = 100.0F}, PlayerMissile);
    const Entity bydo = spawn({.x = 120.0F, .y = 110.0F}, Bydo);

    EXPECT_EQ(step(), (Pairs{{missile, bydo}}));
}

TEST_F(CollisionSystemTest, SeparatedBoxesDoNotCollide)
{
    spawn({.x = 100.0F, .y = 100.0F}, PlayerMissile);
    spawn({.x = 500.0F, .y = 100.0F}, Bydo);

    EXPECT_TRUE(step().empty());
}

TEST_F(CollisionSystemTest, OverlapIsNeededOnBothAxes)
{
    spawn({.x = 100.0F, .y = 100.0F}, PlayerMissile); // Same x, far below
    spawn({.x = 100.0F, .y = 400.0F}, Bydo);

    EXPECT_TRUE(step().empty());
}

TEST_F(CollisionSystemTest, TouchingEdgesDoNotCollide)
{
    // Missile right edge at 108, Bydo left edge at 108: touching, not overlapping
    spawn({.x = 100.0F, .y = 100.0F}, PlayerMissile);
    const Entity bydo = spawn({.x = 132.0F, .y = 100.0F}, Bydo);

    EXPECT_TRUE(step().empty());

    _world.getComponent<Position>(bydo).x = 131.0F; // One unit closer: overlap
    EXPECT_EQ(step().size(), 1U);
}

TEST_F(CollisionSystemTest, LayersThatDoNotMatchNeverCollide)
{
    // A player missile flying through a player ship: no friendly fire
    spawn({.x = 100.0F, .y = 100.0F}, Ship);
    spawn({.x = 100.0F, .y = 100.0F}, PlayerMissile);

    EXPECT_TRUE(step().empty());
}

TEST_F(CollisionSystemTest, OneSideWantingTheOtherIsEnough)
{
    // The ship does not list PlayerMissile... but the "trap" lists PlayerShip
    const Hitbox trap{.halfWidth = 10.0F,
        .halfHeight = 10.0F,
        .layer = layer::None,
        .collidesWith = layer::PlayerShip};
    const Entity ship = spawn({.x = 100.0F, .y = 100.0F}, Ship);
    const Entity trapEntity = spawn({.x = 100.0F, .y = 100.0F}, trap);

    EXPECT_EQ(step(), (Pairs{{ship, trapEntity}}));
}

TEST_F(CollisionSystemTest, EachPairIsReportedOncePerStepWithTheSmallerIdFirst)
{
    const Entity bydo = spawn({.x = 100.0F, .y = 100.0F}, Bydo);
    const Entity missile = spawn({.x = 100.0F, .y = 100.0F}, PlayerMissile);

    const Pairs pairs = step();

    ASSERT_EQ(pairs.size(), 1U);
    EXPECT_EQ(pairs.front().first, bydo); // Created first: smaller id
    EXPECT_EQ(pairs.front().second, missile);
}

TEST_F(CollisionSystemTest, OverlapsAreReportedEveryStepWhileTheyLast)
{
    spawn({.x = 100.0F, .y = 100.0F}, Ship);
    spawn({.x = 100.0F, .y = 100.0F}, Bydo);

    EXPECT_EQ(step().size(), 1U);
    EXPECT_EQ(step().size(), 1U);
}

TEST_F(CollisionSystemTest, ReportsEveryPairInvolvingTheSameEntity)
{
    // One big Bydo touched by a ship and two missiles at once
    const Entity bydo = spawn({.x = 500.0F, .y = 500.0F}, Bydo);
    const Entity ship = spawn({.x = 480.0F, .y = 500.0F}, Ship);
    const Entity missileA = spawn({.x = 510.0F, .y = 495.0F}, PlayerMissile);
    const Entity missileB = spawn({.x = 510.0F, .y = 505.0F}, PlayerMissile);

    // Ship-missile and missile-missile pairs are filtered out by their layers
    EXPECT_EQ(step(), (Pairs{{bydo, ship}, {bydo, missileA}, {bydo, missileB}}));
}

TEST_F(CollisionSystemTest, EntitiesWithoutHitboxAreIgnored)
{
    const Entity scenery = _world.createEntity();
    _world.addComponent(scenery, Position{.x = 100.0F, .y = 100.0F});
    spawn({.x = 100.0F, .y = 100.0F}, Bydo);

    EXPECT_TRUE(step().empty());
}
