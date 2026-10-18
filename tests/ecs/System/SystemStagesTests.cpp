#include "ecs/Scheduler/Scheduler.hpp"
#include "ecs/System/SystemManager.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"

#include <gtest/gtest.h>

#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using ecs::Scheduler;
using ecs::SystemManager;
using ecs::Time;
using ecs::World;

namespace {

using Log = std::vector<std::string>;

/// Writes its name in a shared log when it runs.
template <int Tag>
class NamedSystem final : public ecs::System {
public:
    NamedSystem(Log& log, std::string name)
        : _log(log)
        , _name(std::move(name))
    {
    }

    void update(World& /*world*/, const Time& /*time*/) override
    {
        _log.get().push_back(_name);
    }

private:
    std::reference_wrapper<Log> _log;
    std::string _name;
};

using InputSystem = NamedSystem<0>;
using MovementSystem = NamedSystem<1>;
using CollisionSystem = NamedSystem<2>;
using RenderSystem = NamedSystem<3>;

} // namespace

TEST(SystemStages, SystemsRunStageByStageWhateverTheInsertionOrder)
{
    Log log;
    World world;
    SystemManager systems;
    systems.scheduler().addStageBefore("Input", Scheduler::DefaultStage);
    systems.scheduler().addStage("Collision");
    systems.scheduler().addStage("Render");

    // Added in a scrambled order on purpose
    systems.addToStage<RenderSystem>("Render", log, "render");
    systems.addToStage<CollisionSystem>("Collision", log, "collision");
    systems.add<MovementSystem>(log, "movement");
    systems.addToStage<InputSystem>("Input", log, "input");

    systems.update(world, Time{});

    EXPECT_EQ(log, (Log{"input", "movement", "collision", "render"}));
}

TEST(SystemStages, InsideAStageSystemsRunInInsertionOrder)
{
    Log log;
    World world;
    SystemManager systems;
    systems.add<CollisionSystem>(log, "second");
    systems.add<MovementSystem>(log, "first-added-later");

    systems.update(world, Time{});

    EXPECT_EQ(log, (Log{"second", "first-added-later"}));
}

TEST(SystemStages, AddingToAnUnknownStageThrows)
{
    Log log;
    SystemManager systems;

    EXPECT_THROW(systems.addToStage<InputSystem>("Input", log, "input"), std::invalid_argument);
    EXPECT_FALSE(systems.has<InputSystem>());
}

TEST(SystemStages, StageOfReportsWhereASystemRuns)
{
    Log log;
    SystemManager systems;
    systems.scheduler().addStage("Render");
    systems.add<MovementSystem>(log, "movement");
    systems.addToStage<RenderSystem>("Render", log, "render");

    EXPECT_EQ(systems.stageOf<MovementSystem>(), Scheduler::DefaultStage);
    EXPECT_EQ(systems.stageOf<RenderSystem>(), "Render");
    EXPECT_FALSE(systems.stageOf<InputSystem>().has_value());
}

TEST(SystemStages, StagesAddedLaterTakeTheirPlaceInTheOrder)
{
    Log log;
    World world;
    SystemManager systems;
    systems.add<MovementSystem>(log, "movement");

    systems.scheduler().addStageBefore("Input", Scheduler::DefaultStage);
    systems.addToStage<InputSystem>("Input", log, "input");
    systems.update(world, Time{});

    EXPECT_EQ(log, (Log{"input", "movement"}));
}

TEST(SystemStages, UpdateStageRunsOnlyThatStage)
{
    Log log;
    World world;
    SystemManager systems;
    systems.scheduler().addStage("Render");
    systems.add<MovementSystem>(log, "movement");
    systems.addToStage<RenderSystem>("Render", log, "render");

    systems.updateStage(world, Time{}, "Render");

    EXPECT_EQ(log, Log{"render"});
}

TEST(SystemStages, ClearKeepsTheStages)
{
    Log log;
    SystemManager systems;
    systems.scheduler().addStage("Render");
    systems.addToStage<RenderSystem>("Render", log, "render");

    systems.clear();

    EXPECT_EQ(systems.size(), 0U);
    EXPECT_TRUE(systems.scheduler().hasStage("Render"));
}
