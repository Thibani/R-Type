#include "network/Crypto/SealedDatagram.hpp"
#include "network/Protocol/PacketCodec.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

using namespace network::crypto;
using namespace network::protocol;

namespace {

/// Both ends of a session, as they would be after a real key exchange.
struct Session {
    Session()
    {
        const KeyPair clientKeys = generateKeyPair();
        const KeyPair serverKeys = generateKeyPair();
        client.emplace(
            *deriveSessionKey(clientKeys, serverKeys.publicKey, Role::Client), Role::Client);
        server.emplace(
            *deriveSessionKey(serverKeys, clientKeys.publicKey, Role::Server), Role::Server);
    }

    std::optional<SessionCipher> client;
    std::optional<SessionCipher> server;
};

std::vector<std::byte> encodeOrFail(Payload payload)
{
    const Packet packet{
        .sessionToken = 0xCAFEBABE,
        .sequence = 7,
        .ack = std::nullopt,
        .reliableId = 3,
        .payload = std::move(payload),
    };
    DatagramBuffer buffer{};
    const auto size = encode(packet, buffer);
    if (!size) {
        ADD_FAILURE() << "cannot encode";
        return {};
    }
    return {buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(*size)};
}

std::vector<std::byte> sealOrFail(SessionCipher& cipher, std::span<const std::byte> plain)
{
    DatagramBuffer buffer{};
    const auto size = sealDatagram(cipher, plain, buffer);
    if (!size) {
        ADD_FAILURE() << "cannot seal";
        return {};
    }
    return {buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(*size)};
}

/// True if `needle` appears anywhere in `bytes`.
bool contains(std::span<const std::byte> bytes, std::string_view needle)
{
    const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return text.find(needle) != std::string_view::npos;
}

} // namespace

TEST(SealedDatagram, ClientToServerRoundTrip)
{
    Session session;
    const auto plain = encodeOrFail(Input{.firstTick = 5, .count = 1, .masks = {input::Fire}});

    const auto sealed = sealOrFail(*session.client, plain);
    DatagramBuffer opened{};
    const auto size = openDatagram(*session.server, sealed, opened);

    ASSERT_TRUE(size.has_value());
    const auto decoded = decode(std::span(opened).first(*size), Direction::ClientToServer);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(std::get<Input>(decoded->payload).firstTick, 5U);
    EXPECT_EQ(decoded->sessionToken, 0xCAFEBABEU);
}

TEST(SealedDatagram, ServerToClientRoundTrip)
{
    Session session;
    const auto plain = encodeOrFail(PlayerJoined{.playerId = 1, .name = "Dave"});

    const auto sealed = sealOrFail(*session.server, plain);
    DatagramBuffer opened{};
    const auto size = openDatagram(*session.client, sealed, opened);

    ASSERT_TRUE(size.has_value());
    EXPECT_TRUE(std::equal(plain.begin(), plain.end(), opened.begin()));
    EXPECT_EQ(*size, plain.size());
}

TEST(SealedDatagram, HeaderStaysValidAndCountsTheOverhead)
{
    Session session;
    const auto plain = encodeOrFail(PlayerJoined{.playerId = 1, .name = "Dave"});

    const auto sealed = sealOrFail(*session.server, plain);

    EXPECT_EQ(sealed.size(), plain.size() + Overhead);
    const auto header = decodeHeader(sealed); // The receiver reads it before opening
    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(header->sessionToken, 0xCAFEBABEU);
    EXPECT_EQ(header->payloadSize, plain.size() - HeaderSize + Overhead);
}

TEST(SealedDatagram, PayloadIsNotReadable)
{
    Session session;
    const auto plain = encodeOrFail(PlayerJoined{.playerId = 1, .name = "Dave"});
    ASSERT_TRUE(contains(plain, "Dave"));

    EXPECT_FALSE(contains(sealOrFail(*session.server, plain), "Dave"));
}

TEST(SealedDatagram, ChangingAnyByteMakesItFail)
{
    Session session;
    const auto plain = encodeOrFail(PlayerJoined{.playerId = 1, .name = "Dave"});
    const auto sealed = sealOrFail(*session.server, plain);

    for (std::size_t i = 0; i < sealed.size(); ++i) {
        SCOPED_TRACE(i);
        auto tampered = sealed;
        tampered[i] ^= std::byte{0x01};
        DatagramBuffer opened{};
        EXPECT_FALSE(openDatagram(*session.client, tampered, opened).has_value());
    }

    DatagramBuffer opened{};
    EXPECT_TRUE(openDatagram(*session.client, sealed, opened).has_value()); // Nothing was marked
}

TEST(SealedDatagram, ReplayIsRefused)
{
    Session session;
    const auto sealed = sealOrFail(*session.client, encodeOrFail(Heartbeat{}));
    DatagramBuffer opened{};

    ASSERT_TRUE(openDatagram(*session.server, sealed, opened).has_value());
    const auto replay = openDatagram(*session.server, sealed, opened);

    ASSERT_FALSE(replay.has_value());
    EXPECT_EQ(replay.error(), OpenError::Replayed);
}

TEST(SealedDatagram, AnotherSessionsKeyIsRefused)
{
    Session session;
    Session other;
    const auto sealed = sealOrFail(*session.client, encodeOrFail(Heartbeat{}));
    DatagramBuffer opened{};

    const auto result = openDatagram(*other.server, sealed, opened);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), OpenError::Forged);
}

TEST(SealedDatagram, PlainDatagramIsRefused)
{
    Session session;
    const auto plain = encodeOrFail(Input{.firstTick = 5, .count = 1, .masks = {input::Fire}});
    DatagramBuffer opened{};

    EXPECT_FALSE(openDatagram(*session.server, plain, opened).has_value());
}

TEST(SealedDatagram, LargestSnapshotStillFitsInADatagram)
{
    Session session;
    const auto plain = encodeOrFail(Snapshot{
        .serverTick = 1,
        .lastInputTick = 0,
        .partIndex = 0,
        .partCount = 1,
        .entities = std::vector<EntityState>(MaxSnapshotEntities),
    });

    DatagramBuffer buffer{}; // Exactly what the sender uses: at most 1200 bytes
    const auto size = sealDatagram(*session.server, plain, buffer);

    ASSERT_TRUE(size.has_value());
    EXPECT_LE(*size, MaxDatagramSize);
}

TEST(SealedDatagram, TooShortInputsAreRefused)
{
    Session session;
    const std::vector<std::byte> tooShort(HeaderSize - 1);
    DatagramBuffer buffer{};

    const auto sealed = sealDatagram(*session.client, tooShort, buffer);
    ASSERT_FALSE(sealed.has_value());
    EXPECT_EQ(sealed.error(), SealError::TooSmall);

    const auto opened = openDatagram(*session.server, tooShort, buffer);
    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error(), OpenError::TooSmall);

    const std::vector<std::byte> noRoomForTheTag(HeaderSize + Overhead - 1);
    const auto noTag = openDatagram(*session.server, noRoomForTheTag, buffer);
    ASSERT_FALSE(noTag.has_value());
    EXPECT_EQ(noTag.error(), OpenError::TooSmall);
}

TEST(SealedDatagram, OutputTooSmallIsRefused)
{
    Session session;
    const auto plain = encodeOrFail(Heartbeat{});
    std::vector<std::byte> small(plain.size() + Overhead - 1);

    const auto sealed = sealDatagram(*session.client, plain, small);

    ASSERT_FALSE(sealed.has_value());
    EXPECT_EQ(sealed.error(), SealError::BufferTooSmall);
}
