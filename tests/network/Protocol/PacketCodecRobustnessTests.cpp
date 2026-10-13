// Malformed input must never crash the decoder (docs/protocol.md §7).
// These tests throw truncated, corrupted and random datagrams at it.

#include "network/Protocol/PacketCodec.hpp"

#include "SamplePackets.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

using namespace network::protocol;
using samples::encodeOrFail;

namespace {

/// Whatever the outcome, a datagram the decoder accepts must survive a
/// re-encode / decode cycle unchanged.
void expectConsistent(std::span<const std::byte> datagram, Direction direction)
{
    const auto decoded = decode(datagram, direction);
    if (!decoded) {
        return;
    }
    DatagramBuffer buffer{};
    const auto size = encode(*decoded, buffer);
    ASSERT_TRUE(size.has_value()) << "decoder accepted a packet the encoder refuses";

    const auto again = decode(std::span(buffer).first(*size), direction);
    ASSERT_TRUE(again.has_value());
    EXPECT_EQ(*again, *decoded);
}

} // namespace

TEST(PacketCodecRobustness, EveryTruncationIsRejected)
{
    for (const auto& [packet, direction] : samples::all()) {
        const auto datagram = encodeOrFail(packet);
        for (std::size_t size = 0; size < datagram.size(); ++size) {
            SCOPED_TRACE(size);
            EXPECT_FALSE(decode(std::span(datagram).first(size), direction).has_value());
        }
    }
}

TEST(PacketCodecRobustness, EveryTruncationWithFixedSizeIsRejected)
{
    // Same as above, but payloadSize is patched so the header check passes and
    // the payload parser itself has to detect the truncation.
    for (const auto& [packet, direction] : samples::all()) {
        const auto datagram = encodeOrFail(packet);
        for (std::size_t size = HeaderSize; size < datagram.size(); ++size) {
            SCOPED_TRACE(size);
            std::vector<std::byte> truncated(datagram.begin(), datagram.begin() + static_cast<std::ptrdiff_t>(size));
            const auto payloadSize = static_cast<std::uint16_t>(size - HeaderSize);
            truncated[6] = static_cast<std::byte>(payloadSize >> 8U);
            truncated[7] = static_cast<std::byte>(payloadSize & 0xFFU);

            EXPECT_FALSE(decode(truncated, direction).has_value());
        }
    }
}

TEST(PacketCodecRobustness, SingleBitFlipsNeverBreakTheDecoder)
{
    for (const auto& [packet, direction] : samples::all()) {
        const auto datagram = encodeOrFail(packet);
        for (std::size_t bit = 0; bit < datagram.size() * 8; ++bit) {
            auto corrupted = datagram;
            corrupted[bit / 8] ^= static_cast<std::byte>(1U << (bit % 8));
            expectConsistent(corrupted, direction);
        }
    }
}

TEST(PacketCodecRobustness, RandomDatagramsNeverBreakTheDecoder)
{
    std::mt19937 random(12345); // NOLINT(bugprone-random-generator-seed): reproducible failures
    std::uniform_int_distribution<std::size_t> sizeDistribution(0, MaxDatagramSize + 16);
    std::uniform_int_distribution<int> byteDistribution(0, 255);

    for (int i = 0; i < 20000; ++i) {
        std::vector<std::byte> datagram(sizeDistribution(random));
        for (std::byte& value : datagram) {
            value = static_cast<std::byte>(byteDistribution(random));
        }
        expectConsistent(datagram, Direction::ClientToServer);
        expectConsistent(datagram, Direction::ServerToClient);
    }
}

TEST(PacketCodecRobustness, RandomPayloadsBehindAValidHeaderNeverBreakTheDecoder)
{
    // A random header almost never passes the magic check, so the payload
    // parsers would barely be exercised. Here the header is valid and only
    // the type, flags and payload are random.
    std::mt19937 random(54321); // NOLINT(bugprone-random-generator-seed): reproducible failures
    std::uniform_int_distribution<std::size_t> sizeDistribution(0, 64);
    std::uniform_int_distribution<int> byteDistribution(0, 255);
    std::uniform_int_distribution<std::size_t> typeDistribution(0, samples::all().size() - 1);
    const auto templates = samples::all();

    for (int i = 0; i < 20000; ++i) {
        const auto& [packet, direction] = templates[typeDistribution(random)];
        auto datagram = encodeOrFail(packet);
        datagram.resize(HeaderSize + sizeDistribution(random));
        for (std::size_t index = HeaderSize; index < datagram.size(); ++index) {
            datagram[index] = static_cast<std::byte>(byteDistribution(random));
        }
        const auto payloadSize = static_cast<std::uint16_t>(datagram.size() - HeaderSize);
        datagram[6] = static_cast<std::byte>(payloadSize >> 8U);
        datagram[7] = static_cast<std::byte>(payloadSize & 0xFFU);

        expectConsistent(datagram, direction);
    }
}
