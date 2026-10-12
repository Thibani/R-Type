#include "network/Protocol/PacketCodec.hpp"

#include "SamplePackets.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace network::protocol;
using samples::encodeOrFail;
using samples::makePacket;

namespace {

std::vector<std::byte> bytes(std::initializer_list<int> values)
{
    std::vector<std::byte> result;
    for (const int value : values) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

void setU16(std::vector<std::byte>& datagram, std::size_t offset, std::uint16_t value)
{
    datagram[offset] = static_cast<std::byte>(value >> 8U);
    datagram[offset + 1] = static_cast<std::byte>(value & 0xFFU);
}

/// Appends `extra` to the payload and fixes payloadSize accordingly.
void growPayload(std::vector<std::byte>& datagram, std::size_t extra)
{
    datagram.resize(datagram.size() + extra);
    setU16(datagram, 6, static_cast<std::uint16_t>(datagram.size() - HeaderSize));
}

/// Removes `count` payload bytes and fixes payloadSize accordingly.
void shrinkPayload(std::vector<std::byte>& datagram, std::size_t count)
{
    datagram.resize(datagram.size() - count);
    setU16(datagram, 6, static_cast<std::uint16_t>(datagram.size() - HeaderSize));
}

DecodeError decodeError(const std::vector<std::byte>& datagram, Direction direction)
{
    const auto result = decode(datagram, direction);
    EXPECT_FALSE(result.has_value()) << "datagram was unexpectedly accepted";
    return result ? DecodeError{} : result.error();
}

} // namespace

// --- Wire format --------------------------------------------------------------

TEST(PacketCodec, MatchesTheSpecificationExample)
{
    // docs/protocol.md §10
    const Packet packet{
        .sessionToken = 0,
        .sequence = 0,
        .ack = std::nullopt,
        .reliableId = 0,
        .payload = ConnectRequest{.name = "Thibani"},
    };

    const auto expected = bytes({
        0x52, 0x54, 0x01, 0x01, 0x00, 0x00, 0x00, 0x10, // magic, version, type, flags, reserved, size
        0x00, 0x00, 0x00, 0x00, // sessionToken
        0x00, 0x00, 0x00, 0x00, // sequence, ack
        0x00, 0x00, 0x00, 0x00, // ackBits
        0x54, 0x68, 0x69, 0x62, 0x61, 0x6E, 0x69, 0x00, // "Thibani"
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // padding
    });

    EXPECT_EQ(encodeOrFail(packet), expected);
}

TEST(PacketCodec, EncodesHeaderFields)
{
    const Packet packet = makePacket(PlayerDied{.playerId = 1, .killerNetId = 0x0102}, 0x0A0B);

    const auto datagram = encodeOrFail(packet);

    const auto expected = bytes({
        0x52, 0x54, 0x01, 0x25, // magic, version, PLAYER_DIED
        0x03, 0x00, 0x00, 0x05, // RELIABLE | HAS_ACK, reserved, payloadSize 5
        0xCA, 0xFE, 0xBA, 0xBE, // sessionToken
        0x00, 0x2A, 0x00, 0x29, // sequence 42, ack 41
        0x80, 0x00, 0x00, 0x01, // ackBits
        0x0A, 0x0B, // reliableId
        0x01, 0x01, 0x02, // playerId, killerNetId
    });
    EXPECT_EQ(datagram, expected);
}

TEST(PacketCodec, PayloadSizesMatchTheSpecification)
{
    struct Expected {
        Payload payload;
        std::size_t size;
    };
    Input input{.firstTick = 0, .count = 8, .masks = {}};

    const std::vector<Expected> cases{
        {.payload = ConnectRequest{.name = "a"}, .size = 16},
        {.payload = ConnectAccept{.playerId = 0, .tickRate = 60, .snapshotRate = 30, .maxPlayers = 4}, .size = 4},
        {.payload = ConnectReject(), .size = 1},
        {.payload = Disconnect(), .size = 1},
        {.payload = Heartbeat(), .size = 0},
        {.payload = input, .size = 13},
        {.payload = Snapshot{.serverTick = 0, .lastInputTick = 0, .partIndex = 0, .partCount = 1, .entities = std::vector<EntityState>(3)}, .size = 11 + (3 * 13)},
        {.payload = EntitySpawn(), .size = 14},
        {.payload = EntityDestroy(), .size = 7},
        {.payload = PlayerJoined(), .size = 19},
        {.payload = PlayerLeft(), .size = 4},
        {.payload = PlayerDied(), .size = 5},
        {.payload = GameState(), .size = 7},
    };

    for (const auto& [payload, size] : cases) {
        SCOPED_TRACE(payload.index());
        EXPECT_EQ(encodeOrFail(makePacket(payload)).size(), HeaderSize + size);
    }
}

TEST(PacketCodec, MaximumSnapshotFitsInADatagram)
{
    Snapshot snapshot{.serverTick = 0, .lastInputTick = 0, .partIndex = 0, .partCount = 1, .entities = std::vector<EntityState>(MaxSnapshotEntities)};

    const auto datagram = encodeOrFail(makePacket(snapshot));

    EXPECT_LE(datagram.size(), MaxDatagramSize);
    EXPECT_TRUE(decode(datagram, Direction::ServerToClient).has_value());
}

// --- Round trips --------------------------------------------------------------

TEST(PacketCodec, EveryPacketTypeRoundTrips)
{
    for (const auto& [packet, direction] : samples::all()) {
        SCOPED_TRACE(packet.payload.index());

        const auto decoded = decode(encodeOrFail(packet), direction);

        ASSERT_TRUE(decoded.has_value()) << toString(decoded.error());
        EXPECT_EQ(*decoded, packet);
    }
}

TEST(PacketCodec, AckIsOptional)
{
    Packet packet = makePacket(Heartbeat{});
    packet.ack = std::nullopt;

    const auto decoded = decode(encodeOrFail(packet), Direction::ServerToClient);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_FALSE(decoded->ack.has_value());
}

TEST(PacketCodec, ReliableIdIsOnlySentForReliablePackets)
{
    const Packet packet = makePacket(Heartbeat{}, 1234);

    const auto decoded = decode(encodeOrFail(packet), Direction::ServerToClient);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->reliableId, 0);
}

TEST(PacketCodec, UnknownEntityKindIsKept)
{
    const auto kind = static_cast<EntityKind>(200);
    const Packet packet = makePacket(EntitySpawn{.netId = 5, .kind = kind, .owner = NoPlayer, .x = 0, .y = 0, .vx = 0, .vy = 0}, 1);

    const auto decoded = decode(encodeOrFail(packet), Direction::ServerToClient);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(std::get<EntitySpawn>(decoded->payload).kind, kind);
}

TEST(PacketCodec, NamesUseAllSixteenBytes)
{
    const Packet packet = makePacket(ConnectRequest{.name = "ABCDEFGHIJKLMNOP"});

    const auto decoded = decode(encodeOrFail(packet), Direction::ClientToServer);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(std::get<ConnectRequest>(decoded->payload).name, "ABCDEFGHIJKLMNOP");
}

// --- Header validation (§7 rules 1-6) -----------------------------------------

TEST(PacketCodecValidation, RejectsDatagramSmallerThanHeader)
{
    auto datagram = encodeOrFail(makePacket(Heartbeat{}));
    datagram.pop_back();

    EXPECT_EQ(decodeError(datagram, Direction::ServerToClient), DecodeError::TooSmall);
}

TEST(PacketCodecValidation, RejectsDatagramLargerThanMaximum)
{
    auto datagram = encodeOrFail(makePacket(Heartbeat{}));
    datagram.resize(MaxDatagramSize + 1);

    EXPECT_EQ(decodeError(datagram, Direction::ServerToClient), DecodeError::TooLarge);
}

TEST(PacketCodecValidation, RejectsBadMagic)
{
    auto datagram = encodeOrFail(makePacket(Heartbeat{}));
    datagram[0] = std::byte{'X'};

    EXPECT_EQ(decodeError(datagram, Direction::ServerToClient), DecodeError::BadMagic);
}

TEST(PacketCodecValidation, RejectsReservedByte)
{
    auto datagram = encodeOrFail(makePacket(Heartbeat{}));
    datagram[5] = std::byte{1};

    EXPECT_EQ(decodeError(datagram, Direction::ServerToClient), DecodeError::ReservedBitsSet);
}

TEST(PacketCodecValidation, RejectsUnknownFlag)
{
    auto datagram = encodeOrFail(makePacket(Heartbeat{}));
    datagram[4] |= std::byte{0x80};

    EXPECT_EQ(decodeError(datagram, Direction::ServerToClient), DecodeError::ReservedBitsSet);
}

TEST(PacketCodecValidation, RejectsPayloadSizeMismatch)
{
    auto datagram = encodeOrFail(makePacket(Heartbeat{}));
    setU16(datagram, 6, 4); // Claims 4 bytes, sends 0

    EXPECT_EQ(decodeError(datagram, Direction::ServerToClient), DecodeError::PayloadSizeMismatch);
}

TEST(PacketCodecValidation, RejectsOtherVersionButHeaderStaysReadable)
{
    auto datagram = encodeOrFail(makePacket(ConnectRequest{.name = "old"}));
    datagram[2] = std::byte{2};

    EXPECT_EQ(decodeError(datagram, Direction::ClientToServer), DecodeError::UnsupportedVersion);

    // The server can still tell it was a CONNECT_REQUEST and answer VERSION_MISMATCH.
    const auto header = decodeHeader(datagram);
    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(header->version, 2);
    EXPECT_EQ(header->type, std::to_underlying(PacketType::ConnectRequest));
}

// --- Packet validation (§7 rules 4, 7-9) --------------------------------------

TEST(PacketCodecValidation, RejectsUnknownType)
{
    auto datagram = encodeOrFail(makePacket(Heartbeat{}));
    datagram[3] = std::byte{0x7F};

    EXPECT_EQ(decodeError(datagram, Direction::ServerToClient), DecodeError::UnknownType);
}

TEST(PacketCodecValidation, RejectsWrongDirection)
{
    const Snapshot snapshot{.serverTick = 1, .lastInputTick = 0, .partIndex = 0, .partCount = 1, .entities = {}};
    const auto fromServer = encodeOrFail(makePacket(snapshot));
    const auto fromClient = encodeOrFail(makePacket(ConnectRequest{.name = "x"}));

    EXPECT_EQ(decodeError(fromServer, Direction::ClientToServer), DecodeError::WrongDirection);
    EXPECT_EQ(decodeError(fromClient, Direction::ServerToClient), DecodeError::WrongDirection);
}

TEST(PacketCodecValidation, RejectsMissingReliableFlag)
{
    auto datagram = encodeOrFail(makePacket(PlayerDied{.playerId = 0, .killerNetId = 1}, 3));
    datagram[4] &= ~std::byte{flags::Reliable};

    EXPECT_EQ(decodeError(datagram, Direction::ServerToClient), DecodeError::ReliableFlagMismatch);
}

TEST(PacketCodecValidation, RejectsUnexpectedReliableFlag)
{
    auto datagram = encodeOrFail(makePacket(Heartbeat{}));
    datagram[4] |= std::byte{flags::Reliable};

    EXPECT_EQ(decodeError(datagram, Direction::ServerToClient), DecodeError::ReliableFlagMismatch);
}

TEST(PacketCodecValidation, RejectsTrailingBytes)
{
    auto datagram = encodeOrFail(makePacket(Disconnect{}));
    growPayload(datagram, 1);

    EXPECT_EQ(decodeError(datagram, Direction::ServerToClient), DecodeError::BadPayloadSize);
}

TEST(PacketCodecValidation, RejectsTruncatedPayload)
{
    auto datagram = encodeOrFail(makePacket(GameState{.phase = GamePhase::Running, .score = 1}, 0));
    shrinkPayload(datagram, 1);

    EXPECT_EQ(decodeError(datagram, Direction::ServerToClient), DecodeError::BadPayloadSize);
}

TEST(PacketCodecValidation, RejectsInputCountOutOfRange)
{
    Input input{.firstTick = 0, .count = 1, .masks = {}};
    auto datagram = encodeOrFail(makePacket(input));

    datagram[HeaderSize + 4] = std::byte{0}; // count 0, payload has 1 mask
    EXPECT_EQ(decodeError(datagram, Direction::ClientToServer), DecodeError::BadPayloadSize);

    datagram[HeaderSize + 4] = std::byte{9};
    growPayload(datagram, 8); // 9 masks really present
    EXPECT_EQ(decodeError(datagram, Direction::ClientToServer), DecodeError::BadPayloadSize);
}

TEST(PacketCodecValidation, RejectsInputCountNotMatchingPayload)
{
    Input input{.firstTick = 0, .count = 2, .masks = {}};
    auto datagram = encodeOrFail(makePacket(input));
    datagram[HeaderSize + 4] = std::byte{3}; // Claims 3 masks, sends 2

    EXPECT_EQ(decodeError(datagram, Direction::ClientToServer), DecodeError::BadPayloadSize);
}

TEST(PacketCodecValidation, RejectsReservedInputBits)
{
    Input input{.firstTick = 0, .count = 1, .masks = {}};
    auto datagram = encodeOrFail(makePacket(input));
    datagram[HeaderSize + 5] = std::byte{0x80};

    EXPECT_EQ(decodeError(datagram, Direction::ClientToServer), DecodeError::InvalidValue);
}

TEST(PacketCodecValidation, RejectsSnapshotCountNotMatchingPayload)
{
    const Snapshot snapshot{.serverTick = 0, .lastInputTick = 0, .partIndex = 0, .partCount = 1, .entities = std::vector<EntityState>(2)};
    auto datagram = encodeOrFail(makePacket(snapshot));

    datagram[HeaderSize + 10] = std::byte{255}; // Claims 255 entities, sends 2
    EXPECT_EQ(decodeError(datagram, Direction::ServerToClient), DecodeError::BadPayloadSize);

    datagram[HeaderSize + 10] = std::byte{2};
    growPayload(datagram, 5); // Not a whole entity
    EXPECT_EQ(decodeError(datagram, Direction::ServerToClient), DecodeError::BadPayloadSize);
}

TEST(PacketCodecValidation, RejectsSnapshotPartOutOfRange)
{
    const Snapshot snapshot{.serverTick = 0, .lastInputTick = 0, .partIndex = 0, .partCount = 1, .entities = {}};
    auto datagram = encodeOrFail(makePacket(snapshot));
    datagram[HeaderSize + 8] = std::byte{1}; // partIndex 1 of 1

    EXPECT_EQ(decodeError(datagram, Direction::ServerToClient), DecodeError::InvalidValue);
}

TEST(PacketCodecValidation, RejectsEnumerationOutOfRange)
{
    auto datagram = encodeOrFail(makePacket(Disconnect{}));
    datagram[HeaderSize] = std::byte{5};

    EXPECT_EQ(decodeError(datagram, Direction::ServerToClient), DecodeError::InvalidValue);
}

TEST(PacketCodecValidation, RejectsPlayerIdOutOfRange)
{
    auto datagram = encodeOrFail(makePacket(PlayerLeft{.playerId = 0, .reason = LeaveReason::Quit}, 0));
    datagram[HeaderSize + 2] = std::byte{4};

    EXPECT_EQ(decodeError(datagram, Direction::ServerToClient), DecodeError::InvalidValue);
}

TEST(PacketCodecValidation, RejectsDataAfterNamePadding)
{
    auto datagram = encodeOrFail(makePacket(ConnectRequest{.name = "ab"}));
    datagram[HeaderSize + 10] = std::byte{'x'}; // "ab\0...\0x\0..."

    EXPECT_EQ(decodeError(datagram, Direction::ClientToServer), DecodeError::InvalidName);
}

TEST(PacketCodecValidation, RejectsInvalidUtf8Name)
{
    auto datagram = encodeOrFail(makePacket(ConnectRequest{.name = "ab"}));
    datagram[HeaderSize] = std::byte{0xC3}; // Lead byte followed by 'b', not a continuation

    EXPECT_EQ(decodeError(datagram, Direction::ClientToServer), DecodeError::InvalidName);
}

// --- Encoding failures --------------------------------------------------------

TEST(PacketCodecEncoding, RefusesInvalidPayloads)
{
    const std::vector<Payload> invalid{
        ConnectRequest{.name = "seventeen bytes!!"},
        ConnectRequest{.name = std::string("a\0b", 3)},
        ConnectAccept{.playerId = 4, .tickRate = 60, .snapshotRate = 30, .maxPlayers = 4},
        Input{.firstTick = 0, .count = 0, .masks = {}},
        Input{.firstTick = 0, .count = 9, .masks = {}},
        Snapshot{.serverTick = 0, .lastInputTick = 0, .partIndex = 0, .partCount = 1, .entities = std::vector<EntityState>(MaxSnapshotEntities + 1)},
        PlayerJoined{.playerId = 9, .name = "x"},
        EntitySpawn{.netId = 0, .kind = EntityKind::Bydo, .owner = 7, .x = 0, .y = 0, .vx = 0, .vy = 0},
    };

    for (const Payload& payload : invalid) {
        SCOPED_TRACE(payload.index());
        DatagramBuffer buffer{};
        const auto result = encode(makePacket(payload), buffer);
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), EncodeError::InvalidPayload);
    }
}

TEST(PacketCodecEncoding, RefusesTooSmallBuffer)
{
    std::array<std::byte, HeaderSize + 3> buffer{};

    const auto result = encode(makePacket(GameState{}), buffer);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), EncodeError::BufferTooSmall);
}

TEST(PacketCodecEncoding, ValidatesNames)
{
    EXPECT_TRUE(isValidName(""));
    EXPECT_TRUE(isValidName("Player 1"));
    EXPECT_TRUE(isValidName("Zoë"));
    EXPECT_TRUE(isValidName("日本語"));
    EXPECT_FALSE(isValidName("this name is too long"));
    EXPECT_FALSE(isValidName(std::string("nul\0", 4)));
    EXPECT_FALSE(isValidName("\xC0\xAF")); // Overlong encoding of '/'
    EXPECT_FALSE(isValidName("\xED\xA0\x80")); // UTF-16 surrogate
    EXPECT_FALSE(isValidName("\xF4\x90\x80\x80")); // Above U+10FFFF
    EXPECT_FALSE(isValidName("\xE6\x97")); // Truncated sequence
}
