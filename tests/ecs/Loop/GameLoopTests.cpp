#include "ecs/Loop/GameLoop.hpp"
#include "ecs/System/System.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using ecs::GameLoop;
using ecs::Time;
using ecs::World;

namespace {

struct Call {
    std::string phase;
    Time time;
};

/// Records each call. The Phase tag makes one distinct system type per group,
/// because a SystemManager holds one system of each type.
template <char Phase>
class Recorder final : public ecs::System {
public:
    explicit Recorder(std::vector<Call>& calls)
        : _calls(calls)
    {
    }

    void update(World& /*world*/, const Time& time) override
    {
        _calls.push_back({.phase = std::string(1, Phase), .time = time});
    }

private:
    std::vector<Call>& _calls;
};

std::string phasesOf(const std::vector<Call>& calls)
{
    std::string phases;
    for (const Call& call : calls) {
        phases += call.phase;
    }
    return phases;
}

/// Stops the loop at the end of its `frames`-th frame.
class StopAfter final : public ecs::System {
public:
    StopAfter(GameLoop& loop, int frames)
        : _loop(loop)
        , _framesLeft(frames)
    {
    }

    void update(World& /*world*/, const Time& /*time*/) override
    {
        if (--_framesLeft == 0) {
            _loop.stop();
        }
    }

private:
    GameLoop& _loop;
    int _framesLeft;
};

class Throwing final : public ecs::System {
public:
    void update(World& /*world*/, const Time& /*time*/) override
    {
        throw std::runtime_error("system failed");
    }
};

// A 0.25 s step keeps every duration exact in float.
constexpr GameLoop::Settings QuarterSecond{.stepSeconds = 0.25F};

} // namespace

TEST(GameLoop, FrameSystemsRunOnceAroundTheSimulationSteps)
{
    World world;
    GameLoop loop(world, QuarterSecond);
    std::vector<Call> calls;
    loop.frameStart().add<Recorder<'<'>>(calls);
    world.systems().add<Recorder<'s'>>(calls);
    loop.frameEnd().add<Recorder<'>'>>(calls);

    loop.runFrame(0.5F); // Two steps
    loop.runFrame(0.125F); // No step, frame systems still run

    EXPECT_EQ(phasesOf(calls), "<ss><>");
}

TEST(GameLoop, SimulationAdvancesByTheFixedStep)
{
    World world;
    GameLoop loop(world, QuarterSecond);
    std::vector<Call> calls;
    world.systems().add<Recorder<'s'>>(calls);

    loop.runFrame(0.75F);

    ASSERT_EQ(calls.size(), 3U);
    for (const Call& call : calls) {
        EXPECT_FLOAT_EQ(call.time.deltaSeconds(), 0.25F);
    }
    EXPECT_FLOAT_EQ(calls.back().time.elapsedSeconds(), 0.75F);
    EXPECT_FLOAT_EQ(loop.simulationTime().elapsedSeconds(), 0.75F);
}

TEST(GameLoop, FrameSystemsSeeTheRealFrameDuration)
{
    World world;
    GameLoop loop(world, QuarterSecond);
    std::vector<Call> calls;
    loop.frameEnd().add<Recorder<'>'>>(calls);

    loop.runFrame(0.125F);
    loop.runFrame(0.5F);

    ASSERT_EQ(calls.size(), 2U);
    EXPECT_FLOAT_EQ(calls[1].time.deltaSeconds(), 0.5F);
    EXPECT_FLOAT_EQ(calls[1].time.elapsedSeconds(), 0.625F);
}

TEST(GameLoop, ElapsedTimeDoesNotDriftOverManyFrames)
{
    World world;
    GameLoop loop(world);
    std::vector<Call> calls;
    loop.frameEnd().add<Recorder<'>'>>(calls);
    constexpr int frames = 1'000'000; // One second at a million frames per second

    for (int frame = 0; frame < frames; ++frame) {
        loop.runFrame(1.0F / static_cast<float>(frames));
    }

    // Summing a million float deltas into a float drifts by several percent
    EXPECT_NEAR(calls.back().time.elapsedSeconds(), 1.0F, 1e-4F);
    // The simulation trails real time by less than one step (the leftover)
    EXPECT_NEAR(loop.simulationTime().elapsedSeconds(), 1.0F, 1.0F / 60.0F);
}

TEST(GameLoop, AlphaIsTheProgressIntoTheNextStep)
{
    World world;
    GameLoop loop(world, QuarterSecond);

    loop.runFrame(0.375F); // One step, half of the next one

    EXPECT_FLOAT_EQ(loop.alpha(), 0.5F);
}

TEST(GameLoop, RunReturnsWhenASystemCallsStop)
{
    World world;
    GameLoop loop(world, {.stepSeconds = 0.001F, .sleepUntilNextStep = true});
    std::vector<Call> calls;
    loop.frameStart().add<Recorder<'<'>>(calls);
    loop.frameEnd().add<StopAfter>(loop, 3);

    loop.run();

    EXPECT_EQ(calls.size(), 3U);
    EXPECT_TRUE(loop.stopRequested());
}

TEST(GameLoop, RunDoesNothingOnceStopped)
{
    World world;
    GameLoop loop(world);
    std::vector<Call> calls;
    loop.frameStart().add<Recorder<'<'>>(calls);

    loop.stop();
    loop.run();

    EXPECT_TRUE(calls.empty());
}

TEST(GameLoop, ExceptionFromASystemReachesTheCaller)
{
    World world;
    GameLoop loop(world);
    world.systems().add<Throwing>();

    EXPECT_THROW(loop.runFrame(1.0F), std::runtime_error);
}

TEST(GameLoop, RejectsInvalidSettings)
{
    World world;

    EXPECT_THROW(GameLoop(world, {.stepSeconds = 0.0F}), std::invalid_argument);
    EXPECT_THROW(GameLoop(world, {.maxStepsPerFrame = 0}), std::invalid_argument);
}
