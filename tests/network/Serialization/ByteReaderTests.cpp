#include "network/Serialization/ByteReader.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

using network::ByteReader;

namespace {

template <typename... Bytes>
constexpr std::array<std::byte, sizeof...(Bytes)> bytes(Bytes... values)
{
    return {static_cast<std::byte>(values)...};
}

} // namespace

TEST(ByteReader, ReadsBigEndianIntegers)
{
    const auto buffer = bytes(0x12, 0x34, 0xDE, 0xAD, 0xBE, 0xEF, 0x7F);
    ByteReader reader(buffer);

    EXPECT_EQ(reader.read<std::uint16_t>(), 0x1234U);
    EXPECT_EQ(reader.read<std::uint32_t>(), 0xDEADBEEFU);
    EXPECT_EQ(reader.read<std::uint8_t>(), 0x7FU);
    EXPECT_TRUE(reader.finished());
}

TEST(ByteReader, ReadsNegativeSignedIntegers)
{
    const auto buffer = bytes(0xFF, 0xFE, 0x80, 0x00);
    ByteReader reader(buffer);

    EXPECT_EQ(reader.read<std::int16_t>(), -2);
    EXPECT_EQ(reader.read<std::int16_t>(), -32768);
    EXPECT_TRUE(reader.finished());
}

TEST(ByteReader, OverflowFailsAndReturnsZero)
{
    const auto buffer = bytes(0x01, 0x02, 0x03);
    ByteReader reader(buffer);

    EXPECT_EQ(reader.read<std::uint32_t>(), 0U);
    EXPECT_FALSE(reader.ok());
    EXPECT_EQ(reader.remaining(), 0U);
}

TEST(ByteReader, FailureIsSticky)
{
    const auto buffer = bytes(0x01, 0x02, 0x03);
    ByteReader reader(buffer);

    (void)reader.read<std::uint32_t>(); // Fails
    EXPECT_EQ(reader.read<std::uint8_t>(), 0U); // Would fit, but the reader failed
    EXPECT_FALSE(reader.ok());
    EXPECT_FALSE(reader.finished());
}

TEST(ByteReader, ReadBytesCopiesAndChecksBounds)
{
    const auto buffer = bytes(0xAA, 0xBB, 0xCC);
    ByteReader reader(buffer);

    std::array<std::byte, 2> two{};
    EXPECT_TRUE(reader.readBytes(two));
    EXPECT_EQ(two, bytes(0xAA, 0xBB));

    EXPECT_FALSE(reader.readBytes(two)); // Only one byte left
    EXPECT_FALSE(reader.ok());
}

TEST(ByteReader, FinishedOnlyWhenEverythingWasRead)
{
    const auto buffer = bytes(0x01, 0x02);
    ByteReader reader(buffer);

    (void)reader.read<std::uint8_t>();
    EXPECT_FALSE(reader.finished());
    (void)reader.read<std::uint8_t>();
    EXPECT_TRUE(reader.finished());
}

TEST(ByteReader, EmptyBufferIsFinished)
{
    ByteReader reader({});

    EXPECT_TRUE(reader.finished());
    EXPECT_EQ(reader.read<std::uint8_t>(), 0U);
    EXPECT_FALSE(reader.ok());
}
