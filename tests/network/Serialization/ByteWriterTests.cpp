#include "network/Serialization/ByteReader.hpp"
#include "network/Serialization/ByteWriter.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

using network::ByteReader;
using network::ByteWriter;

TEST(ByteWriter, WritesBigEndianIntegers)
{
    std::array<std::byte, 7> buffer{};
    ByteWriter writer(buffer);

    writer.write(std::uint16_t{0x1234});
    writer.write(std::uint32_t{0xDEADBEEF});
    writer.write(std::uint8_t{0x7F});

    ASSERT_TRUE(writer.ok());
    EXPECT_EQ(writer.size(), 7U);
    const std::array<std::byte, 7> expected{
        std::byte{0x12},
        std::byte{0x34},
        std::byte{0xDE},
        std::byte{0xAD},
        std::byte{0xBE},
        std::byte{0xEF},
        std::byte{0x7F},
    };
    EXPECT_EQ(buffer, expected);
}

TEST(ByteWriter, WritesNegativeSignedIntegersAsTwosComplement)
{
    std::array<std::byte, 2> buffer{};
    ByteWriter writer(buffer);

    writer.write(std::int16_t{-2});

    EXPECT_EQ(buffer[0], std::byte{0xFF});
    EXPECT_EQ(buffer[1], std::byte{0xFE});
}

TEST(ByteWriter, OverflowFailsWithoutWriting)
{
    std::array<std::byte, 3> buffer{};
    ByteWriter writer(buffer);

    writer.write(std::uint32_t{0xFFFFFFFF});

    EXPECT_FALSE(writer.ok());
    EXPECT_EQ(writer.size(), 0U);
    EXPECT_EQ(buffer, (std::array<std::byte, 3>{}));
}

TEST(ByteWriter, FailureIsSticky)
{
    std::array<std::byte, 3> buffer{};
    ByteWriter writer(buffer);

    writer.write(std::uint32_t{1}); // Fails
    writer.write(std::uint8_t{1}); // Would fit, but the writer failed

    EXPECT_FALSE(writer.ok());
    EXPECT_EQ(writer.size(), 0U);
}

TEST(ByteWriter, RoundTripsWithReader)
{
    std::array<std::byte, 16> buffer{};
    ByteWriter writer(buffer);
    writer.write(std::int16_t{-12345});
    writer.write(std::uint32_t{4000000000U});
    writer.write(std::uint8_t{200});

    ByteReader reader{std::span(buffer).first(writer.size())};

    EXPECT_EQ(reader.read<std::int16_t>(), -12345);
    EXPECT_EQ(reader.read<std::uint32_t>(), 4000000000U);
    EXPECT_EQ(reader.read<std::uint8_t>(), 200U);
    EXPECT_TRUE(reader.finished());
}
