#include "ecs/Scheduler/Scheduler.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

using ecs::Scheduler;

namespace {

std::vector<std::string> stagesOf(const Scheduler& scheduler)
{
    return {scheduler.stages().begin(), scheduler.stages().end()};
}

} // namespace

TEST(Scheduler, StartsWithTheDefaultStageOnly)
{
    const Scheduler scheduler;

    EXPECT_EQ(stagesOf(scheduler), std::vector<std::string>{"Update"});
    EXPECT_TRUE(scheduler.hasStage(Scheduler::DefaultStage));
}

TEST(Scheduler, AddStageAppendsAtTheEnd)
{
    Scheduler scheduler;

    scheduler.addStage("Collision");
    scheduler.addStage("Render");

    EXPECT_EQ(stagesOf(scheduler), (std::vector<std::string>{"Update", "Collision", "Render"}));
}

TEST(Scheduler, StagesCanBeInsertedBeforeAndAfterAnotherOne)
{
    Scheduler scheduler;

    scheduler.addStageBefore("Input", "Update");
    scheduler.addStageAfter("Collision", "Update");
    scheduler.addStageAfter("Gameplay", "Input");

    EXPECT_EQ(stagesOf(scheduler),
        (std::vector<std::string>{"Input", "Gameplay", "Update", "Collision"}));
}

TEST(Scheduler, BuildsTheFullGamePipeline)
{
    Scheduler scheduler;

    scheduler.addStageBefore("Input", Scheduler::DefaultStage);
    scheduler.addStageAfter("NetworkReceive", "Input");
    scheduler.addStage("Collision");
    scheduler.addStage("NetworkSend");
    scheduler.addStage("Render");

    EXPECT_EQ(stagesOf(scheduler),
        (std::vector<std::string>{
            "Input", "NetworkReceive", "Update", "Collision", "NetworkSend", "Render"}));
}

TEST(Scheduler, RejectsDuplicateAndEmptyNames)
{
    Scheduler scheduler;
    scheduler.addStage("Render");

    EXPECT_THROW(scheduler.addStage("Render"), std::invalid_argument);
    EXPECT_THROW(scheduler.addStageBefore("Update", "Render"), std::invalid_argument);
    EXPECT_THROW(scheduler.addStage(""), std::invalid_argument);
    EXPECT_EQ(scheduler.stages().size(), 2U);
}

TEST(Scheduler, RejectsUnknownAnchors)
{
    Scheduler scheduler;

    EXPECT_THROW(scheduler.addStageBefore("Input", "DoesNotExist"), std::invalid_argument);
    EXPECT_THROW(scheduler.addStageAfter("Input", "DoesNotExist"), std::invalid_argument);
    EXPECT_FALSE(scheduler.hasStage("Input"));
}
