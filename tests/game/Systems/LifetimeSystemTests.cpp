#include "ecs/Event/Subscription.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Lifetime.hpp"
#include "game/Events/DespawnEvent.hpp"
#include "game/Systems/DespawnSystem.hpp"
#include "game/Systems/LifetimeSystem.hpp"

#include <gtest/gtest.h>

#include <vector>

using ecs::Entity;
using ecs::Time;
using ecs::World;
using game::DespawnEvent;
using game::DespawnReason;
using game::Lifetime;

namespace {

/// A World running the LifetimeSystem and the DespawnSystem, recording every
/// despawn.
class LifetimeSystemTest : public ::testing::Test {
protected:
    LifetimeSystemTest()
        : _onDespawn(_world.events().subscribe<DespawnEvent>(
              [this](const DespawnEvent& event) { _despawns.push_back(event); }))
    {
        _world.systems().add<game::LifetimeSystem>();
        _world.systems().add<game::DespawnSystem>(_world);
    }

    Entity spawn(float seconds)
    {
        const Entity entity = _world.createEntity();
        _world.addComponent(entity, Lifetime{.remainingSeconds = seconds});
        return entity;
    }

    void step(float seconds)
    {
        _world.update(Time{seconds, 0.0F});
    }

    World _world;
    std::vector<DespawnEvent> _despawns;
    ecs::Subscription _onDespawn;
};

} // namespace

TEST_F(LifetimeSystemTest, CountsDownEveryStep)
{
    const Entity missile = spawn(3.0F);

    step(0.5F);
    step(0.25F);

    EXPECT_FLOAT_EQ(_world.getComponent<Lifetime>(missile).remainingSeconds, 2.25F);
    EXPECT_TRUE(_despawns.empty());
}

TEST_F(LifetimeSystemTest, ExpiredEntitiesAreDespawnedWithTheReasonExpired)
{
    const Entity missile = spawn(1.0F);

    step(1.0F);

    ASSERT_EQ(_despawns.size(), 1U);
    EXPECT_EQ(_despawns.front().entity, missile);
    EXPECT_EQ(_despawns.front().reason, DespawnReason::Expired);
    EXPECT_FALSE(_despawns.front().killer.valid());
    EXPECT_FALSE(_world.isAlive(missile));
}

TEST_F(LifetimeSystemTest, ExpiresOnTheStepWhereTheTimeRunsOut)
{
    const Entity missile = spawn(0.1F);

    for (int i = 0; i < 5; ++i) { // 5 × 1/60 s ≈ 0.083 s
        step(1.0F / 60.0F);
    }
    EXPECT_TRUE(_world.isAlive(missile));

    step(1.0F / 60.0F); // ≈ 0.1 s
    step(1.0F / 60.0F); // Float rounding: at the latest one step later
    EXPECT_FALSE(_world.isAlive(missile));
    EXPECT_EQ(_despawns.size(), 1U);
}

TEST_F(LifetimeSystemTest, OnlyExpiredEntitiesAreDespawned)
{
    const Entity shortLived = spawn(1.0F);
    const Entity longLived = spawn(5.0F);
    const Entity forever = _world.createEntity(); // No Lifetime

    step(2.0F);

    EXPECT_FALSE(_world.isAlive(shortLived));
    EXPECT_TRUE(_world.isAlive(longLived));
    EXPECT_TRUE(_world.isAlive(forever));
}
