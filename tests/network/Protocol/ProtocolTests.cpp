#include "network/Protocol/Protocol.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

using network::protocol::dequantize;
using network::protocol::quantize;

TEST(Quantization, UsesQuarterUnits)
{
    EXPECT_EQ(quantize(1.0F), 4);
    EXPECT_EQ(quantize(-2.5F), -10);
    EXPECT_FLOAT_EQ(dequantize(4), 1.0F);
    EXPECT_FLOAT_EQ(dequantize(-10), -2.5F);
}

TEST(Quantization, RoundsToNearestStep)
{
    EXPECT_EQ(quantize(0.1F), 0);
    EXPECT_EQ(quantize(0.2F), 1);
    EXPECT_EQ(quantize(100.37F), 401);
}

TEST(Quantization, ClampsOutOfRangeValues)
{
    EXPECT_EQ(quantize(1.0e9F), std::numeric_limits<std::int16_t>::max());
    EXPECT_EQ(quantize(-1.0e9F), std::numeric_limits<std::int16_t>::min());
    EXPECT_EQ(quantize(std::numeric_limits<float>::infinity()),
        std::numeric_limits<std::int16_t>::max());
}

TEST(Quantization, NanBecomesZero)
{
    EXPECT_EQ(quantize(std::numeric_limits<float>::quiet_NaN()), 0);
}

TEST(Quantization, CoversTheWholePlayfield)
{
    for (const float value : {0.0F, 960.25F, 1919.75F, 1080.0F, -64.0F, 2200.5F}) {
        EXPECT_FLOAT_EQ(dequantize(quantize(value)), value);
    }
}
