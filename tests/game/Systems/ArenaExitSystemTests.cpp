#include "ecs/Event/Subscription.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Components/ArenaExit.hpp"
#include "game/Components/Motion.hpp"
#include "game/Events/DespawnEvent.hpp"
#include "game/Systems/ArenaExitSystem.hpp"
#include "game/Systems/DespawnSystem.hpp"
#include "game/Systems/MovementSystem.hpp"

#include <gtest/gtest.h>

#include <vector>

using ecs::Entity;
using ecs::Time;
using ecs::World;
using game::DespawnEvent;
using game::DespawnOutsideArena;
using game::DespawnReason;
using game::Position;
using game::Velocity;
namespace arena = game::arena;

namespace {

constexpr float Margin = 100.0F;

/// A World running movement, the ArenaExitSystem and the DespawnSystem, as
/// the game does, recording every despawn.
class ArenaExitSystemTest : public ::testing::Test {
protected:
    ArenaExitSystemTest()
        : _onDespawn(_world.events().subscribe<DespawnEvent>(
              [this](const DespawnEvent& event) { _despawns.push_back(event); }))
    {
        _world.systems().add<game::MovementSystem>();
        _world.systems().add<game::ArenaExitSystem>();
        _world.systems().add<game::DespawnSystem>(_world);
    }

    Entity spawn(Position position, Velocity velocity = {})
    {
        const Entity entity = _world.createEntity();
        _world.addComponent(entity, position);
        _world.addComponent(entity, velocity);
        _world.addComponent(entity, DespawnOutsideArena{.margin = Margin});
        return entity;
    }

    void step(float seconds = 1.0F / 60.0F)
    {
        _world.update(Time{seconds, 0.0F});
    }

    World _world;
    std::vector<DespawnEvent> _despawns;
    ecs::Subscription _onDespawn;
};

} // namespace

TEST_F(ArenaExitSystemTest, EntitiesInsideTheArenaStay)
{
    const Entity bydo = spawn({.x = 960.0F, .y = 540.0F});

    step();

    EXPECT_TRUE(_world.isAlive(bydo));
    EXPECT_TRUE(_despawns.empty());
}

TEST_F(ArenaExitSystemTest, EntitiesWithinTheMarginStay)
{
    // Just off screen on every side: enemies waiting to fly in, missiles leaving
    const Entity left = spawn({.x = -Margin, .y = 540.0F});
    const Entity right = spawn({.x = arena::Width + Margin, .y = 540.0F});
    const Entity top = spawn({.x = 960.0F, .y = -Margin});
    const Entity bottom = spawn({.x = 960.0F, .y = arena::Height + Margin});

    step();

    EXPECT_TRUE(_world.isAlive(left));
    EXPECT_TRUE(_world.isAlive(right));
    EXPECT_TRUE(_world.isAlive(top));
    EXPECT_TRUE(_world.isAlive(bottom));
}

TEST_F(ArenaExitSystemTest, EntitiesBeyondTheMarginAreDespawnedOnEverySide)
{
    const Entity left = spawn({.x = -Margin - 1.0F, .y = 540.0F});
    const Entity right = spawn({.x = arena::Width + Margin + 1.0F, .y = 540.0F});
    const Entity top = spawn({.x = 960.0F, .y = -Margin - 1.0F});
    const Entity bottom = spawn({.x = 960.0F, .y = arena::Height + Margin + 1.0F});

    step();

    std::vector<Entity> despawned;
    for (const DespawnEvent& event : _despawns) {
        EXPECT_EQ(event.reason, DespawnReason::OutOfBounds);
        despawned.push_back(event.entity);
    }
    EXPECT_EQ(despawned, (std::vector<Entity>{left, right, top, bottom}));
    EXPECT_EQ(_world.entityCount(), 0U);
}

TEST_F(ArenaExitSystemTest, AMissileFlyingRightIsDespawnedOnceItHasLeft)
{
    // 1200 units/s from the right edge: past the margin after 100/1200 s ≈ 5 steps
    const Entity missile = spawn({.x = arena::Width, .y = 540.0F}, {.dx = 1200.0F, .dy = 0.0F});

    for (int i = 0; i < 4; ++i) {
        step();
    }
    EXPECT_TRUE(_world.isAlive(missile));

    step();
    step();
    EXPECT_FALSE(_world.isAlive(missile));
    EXPECT_EQ(_despawns.size(), 1U);
}

TEST_F(ArenaExitSystemTest, EntitiesWithoutTheComponentAreNeverDespawned)
{
    const Entity scenery = _world.createEntity();
    _world.addComponent(scenery, Position{.x = -10000.0F, .y = -10000.0F});

    step();

    EXPECT_TRUE(_world.isAlive(scenery));
}

TEST(DespawnOutsideArenaTest, HasAMarginByDefault)
{
    EXPECT_FLOAT_EQ(DespawnOutsideArena{}.margin, DespawnOutsideArena::DefaultMargin);
}
