#include "ecs/Time/Time.hpp"

#include <gtest/gtest.h>

using ecs::Time;

TEST(Time, DefaultIsTheStartOfTheSimulation)
{
    constexpr Time time;

    static_assert(time.deltaSeconds() == 0.0F);
    static_assert(time.elapsedSeconds() == 0.0F);
}

TEST(Time, StoresDeltaAndElapsed)
{
    constexpr Time time{0.016F, 2.5F};

    EXPECT_FLOAT_EQ(time.deltaSeconds(), 0.016F);
    EXPECT_FLOAT_EQ(time.elapsedSeconds(), 2.5F);
}

TEST(Time, AdvancedAccumulatesElapsedTime)
{
    Time time;

    time = time.advanced(0.5F);
    time = time.advanced(0.25F);

    EXPECT_FLOAT_EQ(time.deltaSeconds(), 0.25F);
    EXPECT_FLOAT_EQ(time.elapsedSeconds(), 0.75F);
}
