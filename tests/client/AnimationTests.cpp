// Sprite animations (client/Ui/Animation.hpp): which frame is shown when.

#include "client/Ui/Animation.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

TEST(Animation, PingPongGoesThereAndBackWithoutRepeatingTheEnds)
{
    std::vector<std::size_t> frames;
    frames.reserve(9);
    for (int step = 0; step < 9; ++step) {
        // Middle of each frame's time slot, at 1 frame per second
        frames.push_back(client::pingPongFrame(4, 1.0F, static_cast<float>(step) + 0.5F));
    }

    EXPECT_EQ(frames, (std::vector<std::size_t>{0, 1, 2, 3, 2, 1, 0, 1, 2}));
}

TEST(Animation, FrameRateSetsTheSpeed)
{
    EXPECT_EQ(client::pingPongFrame(32, 10.0F, 0.05F), 0U);
    EXPECT_EQ(client::pingPongFrame(32, 10.0F, 0.15F), 1U);
    EXPECT_EQ(client::pingPongFrame(32, 10.0F, 3.15F), 31U); // The last frame...
    EXPECT_EQ(client::pingPongFrame(32, 10.0F, 3.25F), 30U); // ...then back
}

TEST(Animation, ASingleFrameOrNoTimeShowsTheFirstFrame)
{
    EXPECT_EQ(client::pingPongFrame(1, 10.0F, 5.0F), 0U);
    EXPECT_EQ(client::pingPongFrame(0, 10.0F, 5.0F), 0U);
    EXPECT_EQ(client::pingPongFrame(8, 10.0F, 0.0F), 0U);
    EXPECT_EQ(client::pingPongFrame(8, 0.0F, 5.0F), 0U);
}
