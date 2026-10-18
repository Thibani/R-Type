#include "ecs/Event/Subscription.hpp"
#include "ecs/Scheduler/Scheduler.hpp"
#include "ecs/System/System.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using ecs::Entity;
using ecs::Scheduler;
using ecs::Time;
using ecs::World;

namespace {

struct Health {
    int current{};
};

struct KilledEvent {
    Entity entity;
};

/// Stage "Gameplay": kills every entity, through commands and an event.
class KillSystem final : public ecs::System {
public:
    void update(World& world, const Time& /*time*/) override
    {
        world.query<Health>([&](Entity entity, Health&) {
            world.commands().destroyEntity(entity);
            world.events().emit(KilledEvent{.entity = entity});
        });
    }
};

/// Stage "Report": runs after "Gameplay" and records what it sees.
class ReportSystem final : public ecs::System {
public:
    explicit ReportSystem(World& world)
        : _onKilled(
              world.events().subscribe<KilledEvent>([this](const KilledEvent&) { ++killsHeard; }))
    {
    }

    void update(World& world, const Time& /*time*/) override
    {
        killsHeardBeforeUpdate = killsHeard;
        world.query<Health>([&](Entity, Health&) { ++entitiesSeen; });
    }

    int killsHeard{0};
    int killsHeardBeforeUpdate{-1};
    int entitiesSeen{0};

private:
    ecs::Subscription _onKilled;
};

void setUpStages(World& world)
{
    world.systems().scheduler().addStageBefore("Gameplay", Scheduler::DefaultStage);
    world.systems().scheduler().addStage("Report");
}

} // namespace

TEST(WorldStages, CommandsOfAStageAreAppliedBeforeTheNextStage)
{
    World world;
    setUpStages(world);
    world.systems().addToStage<KillSystem>("Gameplay");
    auto& report = world.systems().addToStage<ReportSystem>("Report", world);
    for (int index = 0; index < 3; ++index) {
        world.addComponent(world.createEntity(), Health{});
    }

    world.update(Time{});

    EXPECT_EQ(report.entitiesSeen, 0); // Already destroyed when "Report" runs
    EXPECT_EQ(world.entityCount(), 0U);
}

TEST(WorldStages, EventsOfAStageAreDeliveredBeforeTheNextStage)
{
    World world;
    setUpStages(world);
    world.systems().addToStage<KillSystem>("Gameplay");
    auto& report = world.systems().addToStage<ReportSystem>("Report", world);
    for (int index = 0; index < 3; ++index) {
        world.addComponent(world.createEntity(), Health{});
    }

    world.update(Time{});

    EXPECT_EQ(report.killsHeardBeforeUpdate, 3); // Delivered between the two stages
}

TEST(WorldStages, StagesRunInSchedulerOrderNotInsertionOrder)
{
    World world;
    setUpStages(world);
    // "Report" is added first, but its stage comes after "Gameplay"
    auto& report = world.systems().addToStage<ReportSystem>("Report", world);
    world.systems().addToStage<KillSystem>("Gameplay");
    world.addComponent(world.createEntity(), Health{});

    world.update(Time{});

    EXPECT_EQ(report.entitiesSeen, 0);
}
