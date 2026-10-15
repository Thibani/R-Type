#include "ecs/Time/FixedTimestep.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

using ecs::FixedTimestep;

// Most tests use a 0.25 s step: multiples of 1/8 are exact in float, so the
// expected step counts are not blurred by rounding.

TEST(FixedTimestep, NothingElapsedRunsNoStep)
{
    FixedTimestep timestep(0.25F);

    EXPECT_EQ(timestep.advance(0.0F), 0);
    EXPECT_FLOAT_EQ(timestep.alpha(), 0.0F);
}

TEST(FixedTimestep, OneStepOfTimeRunsOneStep)
{
    FixedTimestep timestep(0.25F);

    EXPECT_EQ(timestep.advance(0.25F), 1);
}

TEST(FixedTimestep, ShortFramesAccumulateUntilAStepIsFull)
{
    FixedTimestep timestep(0.25F);

    EXPECT_EQ(timestep.advance(0.125F), 0);
    EXPECT_EQ(timestep.advance(0.125F), 1);
}

TEST(FixedTimestep, LongFrameRunsSeveralSteps)
{
    FixedTimestep timestep(0.25F);

    EXPECT_EQ(timestep.advance(0.75F), 3);
}

TEST(FixedTimestep, LeftoverTimeIsKeptForTheNextFrame)
{
    FixedTimestep timestep(0.25F);

    EXPECT_EQ(timestep.advance(0.375F), 1); // 0.125 s left over
    EXPECT_FLOAT_EQ(timestep.alpha(), 0.5F);
    EXPECT_EQ(timestep.advance(0.125F), 1); // 0.125 + 0.125 fills a step
    EXPECT_FLOAT_EQ(timestep.alpha(), 0.0F);
}

TEST(FixedTimestep, FreezeIsCappedAndItsBacklogDropped)
{
    FixedTimestep timestep(0.25F, 4);

    EXPECT_EQ(timestep.advance(10.0F), 4); // 40 steps worth, only 4 run
    EXPECT_EQ(timestep.advance(0.0F), 0); // the other 36 are not replayed later
}

TEST(FixedTimestep, InvalidDurationsCountAsNothing)
{
    FixedTimestep timestep(0.25F);

    EXPECT_EQ(timestep.advance(-1.0F), 0);
    EXPECT_EQ(timestep.advance(std::numeric_limits<float>::quiet_NaN()), 0);
    EXPECT_EQ(timestep.advance(std::numeric_limits<float>::infinity()), 5); // capped like a freeze
    EXPECT_EQ(timestep.advance(0.25F), 1); // still works afterwards
}

TEST(FixedTimestep, SixtyHertzGivesSixtyStepsPerSecond)
{
    FixedTimestep timestep(1.0F / 60.0F);
    int steps = 0;

    for (int frame = 0; frame < 144; ++frame) { // One second at 144 FPS
        steps += timestep.advance(1.0F / 144.0F);
    }

    // Float rounding may leave the 60th step a hair short of complete
    EXPECT_GE(steps, 59);
    EXPECT_LE(steps, 60);
}

TEST(FixedTimestep, SameFramesGiveSameSteps)
{
    FixedTimestep first(1.0F / 60.0F);
    FixedTimestep second(1.0F / 60.0F);

    for (const float frame : {0.007F, 0.031F, 0.016F, 0.2F, 0.0F, 0.019F}) {
        EXPECT_EQ(first.advance(frame), second.advance(frame));
    }
}

TEST(FixedTimestep, RejectsNonPositiveSettings)
{
    EXPECT_THROW(FixedTimestep(0.0F), std::invalid_argument);
    EXPECT_THROW(FixedTimestep(-0.1F), std::invalid_argument);
    EXPECT_THROW(FixedTimestep(0.25F, 0), std::invalid_argument);
}

TEST(FixedTimestep, UsableAtCompileTime)
{
    constexpr int steps = [] {
        FixedTimestep timestep(0.25F);
        return timestep.advance(0.5F);
    }();

    static_assert(steps == 2);
}
