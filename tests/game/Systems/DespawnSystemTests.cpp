#include "ecs/Event/Subscription.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "game/Events/DespawnEvent.hpp"
#include "game/Systems/DespawnSystem.hpp"

#include <gtest/gtest.h>

#include <vector>

using ecs::Entity;
using ecs::Time;
using ecs::World;
using game::DespawnEvent;
using game::DespawnReason;
using game::DespawnRequest;
using game::Position;

namespace {

/// A World running the DespawnSystem, recording every despawn.
class DespawnSystemTest : public ::testing::Test {
protected:
    DespawnSystemTest()
        : _onDespawn(_world.events().subscribe<DespawnEvent>(
              [this](const DespawnEvent& event) { _despawns.push_back(event); }))
    {
        _world.systems().add<game::DespawnSystem>(_world);
    }

    void request(Entity entity, DespawnReason reason, Entity killer = ecs::NullEntity)
    {
        _world.events().emit(DespawnRequest{.entity = entity, .reason = reason, .killer = killer});
    }

    void step()
    {
        _world.update(Time{1.0F / 60.0F, 0.0F});
    }

    World _world;
    std::vector<DespawnEvent> _despawns;
    ecs::Subscription _onDespawn;
};

} // namespace

TEST_F(DespawnSystemTest, ARequestIsAnnouncedThenTheEntityIsDestroyed)
{
    const Entity bydo = _world.createEntity();
    const Entity missile = _world.createEntity();

    request(bydo, DespawnReason::Killed, missile);
    step();

    ASSERT_EQ(_despawns.size(), 1U);
    EXPECT_EQ(_despawns.front().entity, bydo);
    EXPECT_EQ(_despawns.front().reason, DespawnReason::Killed);
    EXPECT_EQ(_despawns.front().killer, missile);
    EXPECT_FALSE(_world.isAlive(bydo));
    EXPECT_TRUE(_world.isAlive(missile));
}

TEST_F(DespawnSystemTest, ListenersSeeTheEntityAliveWithItsComponents)
{
    const Entity bydo = _world.createEntity();
    _world.addComponent(bydo, Position{.x = 12.0F, .y = 34.0F});

    bool wasAlive = false;
    float seenX = 0.0F;
    const ecs::Subscription reader
        = _world.events().subscribe<DespawnEvent>([&](const DespawnEvent& event) {
              wasAlive = _world.isAlive(event.entity);
              seenX = _world.getComponent<Position>(event.entity).x;
          });

    request(bydo, DespawnReason::OutOfBounds);
    step();

    EXPECT_TRUE(wasAlive);
    EXPECT_FLOAT_EQ(seenX, 12.0F);
    EXPECT_FALSE(_world.isAlive(bydo));
}

TEST_F(DespawnSystemTest, SeveralRequestsInOneStepAreAnnouncedOnceWithTheFirstReason)
{
    const Entity missile = _world.createEntity();

    request(missile, DespawnReason::Expired);
    request(missile, DespawnReason::OutOfBounds);
    step();

    ASSERT_EQ(_despawns.size(), 1U);
    EXPECT_EQ(_despawns.front().reason, DespawnReason::Expired);
}

TEST_F(DespawnSystemTest, RequestsForDeadEntitiesAreIgnored)
{
    const Entity missile = _world.createEntity();
    _world.destroyEntity(missile);

    request(missile, DespawnReason::Expired);
    step();

    EXPECT_TRUE(_despawns.empty());
}

TEST_F(DespawnSystemTest, AReusedIdIsNotMistakenForTheDespawnedEntity)
{
    const Entity first = _world.createEntity();
    request(first, DespawnReason::Expired);
    step();

    const Entity second = _world.createEntity(); // May reuse the id of `first`
    request(second, DespawnReason::OutOfBounds);
    step();

    ASSERT_EQ(_despawns.size(), 2U);
    EXPECT_EQ(_despawns.back().entity, second);
    EXPECT_FALSE(_world.isAlive(second));
}

TEST(DespawnReasonTest, MatchesTheProtocolValues)
{
    // docs/protocol.md, ENTITY_DESTROY: 0 KILLED, 1 OUT_OF_BOUNDS, 2 EXPIRED
    EXPECT_EQ(static_cast<int>(DespawnReason::Killed), 0);
    EXPECT_EQ(static_cast<int>(DespawnReason::OutOfBounds), 1);
    EXPECT_EQ(static_cast<int>(DespawnReason::Expired), 2);
}
