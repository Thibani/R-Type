#include "ecs/Time/FrameClock.hpp"

#include <gtest/gtest.h>

#include <chrono>

namespace {

/// Clock moved by hand, so the tests never sleep.
struct FakeClock {
    using duration = std::chrono::milliseconds;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = std::chrono::time_point<FakeClock>;

    static time_point now() noexcept
    {
        return time_point(duration(milliseconds));
    }

    static inline rep milliseconds = 0; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
};

class FrameClockTest : public testing::Test {
protected:
    void SetUp() override
    {
        FakeClock::milliseconds = 0;
    }
};

} // namespace

TEST_F(FrameClockTest, TickReturnsTimeSinceConstruction)
{
    ecs::BasicFrameClock<FakeClock> clock;

    FakeClock::milliseconds += 250;

    EXPECT_FLOAT_EQ(clock.tick(), 0.25F);
}

TEST_F(FrameClockTest, TickReturnsTimeSincePreviousTick)
{
    ecs::BasicFrameClock<FakeClock> clock;

    FakeClock::milliseconds += 500;
    (void)clock.tick();
    FakeClock::milliseconds += 125;

    EXPECT_FLOAT_EQ(clock.tick(), 0.125F);
    EXPECT_FLOAT_EQ(clock.tick(), 0.0F); // No time passed since the last tick
}

TEST(FrameClock, RealClockNeverGoesBackwards)
{
    ecs::FrameClock clock;

    EXPECT_GE(clock.tick(), 0.0F);
    EXPECT_GE(clock.tick(), 0.0F);
}
