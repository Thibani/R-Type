#include "network/Protocol/PacketCodec.hpp"

#include "network/Serialization/ByteReader.hpp"
#include "network/Serialization/ByteWriter.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <utility>
#include <variant>

namespace network::protocol {

namespace {

    // --- Value checks ---------------------------------------------------------

    template <typename Enum>
    constexpr bool inRange(Enum value, Enum first, Enum last) noexcept
    {
        return std::to_underlying(value) >= std::to_underlying(first)
            && std::to_underlying(value) <= std::to_underlying(last);
    }

    constexpr bool isValidPlayer(std::uint8_t playerId) noexcept
    {
        return playerId < MaxPlayers;
    }

    constexpr bool isValidOwner(std::uint8_t owner) noexcept
    {
        return isValidPlayer(owner) || owner == NoPlayer;
    }

    /// Strict UTF-8 check: rejects truncated sequences, overlong encodings,
    /// UTF-16 surrogates and code points above U+10FFFF.
    bool isValidUtf8(std::string_view text) noexcept
    {
        static constexpr std::array<std::uint32_t, 5> SmallestCodePoint{0, 0, 0x80, 0x800, 0x10000};

        std::size_t index = 0;
        while (index < text.size()) {
            const auto lead = static_cast<unsigned char>(text[index]);
            std::size_t length = 0;
            std::uint32_t codePoint = 0;

            if (lead < 0x80U) {
                ++index;
                continue;
            }
            if ((lead & 0xE0U) == 0xC0U) {
                length = 2;
                codePoint = lead & 0x1FU;
            } else if ((lead & 0xF0U) == 0xE0U) {
                length = 3;
                codePoint = lead & 0x0FU;
            } else if ((lead & 0xF8U) == 0xF0U) {
                length = 4;
                codePoint = lead & 0x07U;
            } else {
                return false; // Continuation byte or invalid lead byte
            }

            if (length > text.size() - index) {
                return false;
            }
            for (std::size_t i = 1; i < length; ++i) {
                const auto continuation = static_cast<unsigned char>(text[index + i]);
                if ((continuation & 0xC0U) != 0x80U) {
                    return false;
                }
                codePoint = (codePoint << 6U) | (continuation & 0x3FU);
            }

            const bool isSurrogate = codePoint >= 0xD800U && codePoint <= 0xDFFFU;
            if (codePoint < SmallestCodePoint[length] || codePoint > 0x10FFFFU || isSurrogate) {
                return false;
            }
            index += length;
        }
        return true;
    }

    // Every rule checked here is checked both before encoding and after
    // decoding, so the encoder never produces what the decoder rejects.

    bool validate(const ConnectRequest& packet) noexcept
    {
        return isValidName(packet.name);
    }

    bool validate(const ConnectAccept& packet) noexcept
    {
        return packet.maxPlayers >= 1 && packet.maxPlayers <= MaxPlayers
            && packet.playerId < packet.maxPlayers && packet.tickRate > 0
            && packet.snapshotRate > 0;
    }

    bool validate(const ConnectReject& packet) noexcept
    {
        return inRange(packet.reason, RejectReason::ServerFull, RejectReason::InvalidName);
    }

    bool validate(const Disconnect& packet) noexcept
    {
        return inRange(packet.reason, DisconnectReason::Quit, DisconnectReason::ProtocolError);
    }

    bool validate(const Heartbeat& /*packet*/) noexcept
    {
        return true;
    }

    bool validate(const Input& packet) noexcept
    {
        if (packet.count < 1 || packet.count > MaxInputMasks) {
            return false;
        }
        return std::ranges::all_of(std::span(packet.masks).first(packet.count),
            [](std::uint8_t mask) { return (mask & ~unsigned{input::Known}) == 0U; });
    }

    bool validate(const Snapshot& packet) noexcept
    {
        if (packet.partCount == 0 || packet.partIndex >= packet.partCount
            || packet.entities.size() > MaxSnapshotEntities) {
            return false;
        }
        return std::ranges::all_of(
            packet.entities, [](const EntityState& entity) { return isValidOwner(entity.owner); });
    }

    bool validate(const EntitySpawn& packet) noexcept
    {
        return isValidOwner(packet.owner);
    }

    bool validate(const EntityDestroy& packet) noexcept
    {
        return inRange(packet.reason, DestroyReason::Killed, DestroyReason::Expired);
    }

    bool validate(const PlayerJoined& packet) noexcept
    {
        return isValidPlayer(packet.playerId) && isValidName(packet.name);
    }

    bool validate(const PlayerLeft& packet) noexcept
    {
        return isValidPlayer(packet.playerId)
            && inRange(packet.reason, LeaveReason::Quit, LeaveReason::Kicked);
    }

    bool validate(const PlayerDied& packet) noexcept
    {
        return isValidPlayer(packet.playerId);
    }

    bool validate(const GameState& packet) noexcept
    {
        return inRange(packet.phase, GamePhase::WaitingForPlayers, GamePhase::GameOver);
    }

    // --- Names (§2.2) ---------------------------------------------------------

    /// Writes `name` as a 16-byte zero-padded field. `name` must be valid.
    void writeName(ByteWriter& writer, std::string_view name) noexcept
    {
        std::array<std::byte, NameSize> field{};
        for (std::size_t i = 0; i < name.size() && i < NameSize; ++i) {
            field[i] = static_cast<std::byte>(name[i]);
        }
        writer.writeBytes(field);
    }

    /// Reads a 16-byte name field. Returns false if the field is not valid
    /// zero-padded UTF-8. A truncated field is left to the caller (reader not ok).
    bool readName(ByteReader& reader, std::string& name)
    {
        std::array<std::byte, NameSize> field{};
        if (!reader.readBytes(field)) {
            return true;
        }

        std::size_t length = 0;
        while (length < NameSize && field[length] != std::byte{0}) {
            ++length;
        }
        for (std::size_t i = length; i < NameSize; ++i) {
            if (field[i] != std::byte{0}) {
                return false; // Data after the padding started
            }
        }

        name.resize(length);
        for (std::size_t i = 0; i < length; ++i) {
            name[i] = static_cast<char>(std::to_integer<unsigned char>(field[i]));
        }
        return isValidUtf8(name);
    }

    // --- Payload writers ------------------------------------------------------

    void writePayload(ByteWriter& writer, const ConnectRequest& packet) noexcept
    {
        writeName(writer, packet.name);
    }

    void writePayload(ByteWriter& writer, const ConnectAccept& packet) noexcept
    {
        writer.write(packet.playerId);
        writer.write(packet.tickRate);
        writer.write(packet.snapshotRate);
        writer.write(packet.maxPlayers);
    }

    void writePayload(ByteWriter& writer, const ConnectReject& packet) noexcept
    {
        writer.write(std::to_underlying(packet.reason));
    }

    void writePayload(ByteWriter& writer, const Disconnect& packet) noexcept
    {
        writer.write(std::to_underlying(packet.reason));
    }

    void writePayload(ByteWriter& /*writer*/, const Heartbeat& /*packet*/) noexcept { }

    void writePayload(ByteWriter& writer, const Input& packet) noexcept
    {
        writer.write(packet.firstTick);
        writer.write(packet.count);
        for (std::size_t i = 0; i < packet.count; ++i) {
            writer.write(packet.masks[i]);
        }
    }

    void writeEntityState(ByteWriter& writer, const EntityState& entity) noexcept
    {
        writer.write(entity.netId);
        writer.write(std::to_underlying(entity.kind));
        writer.write(entity.owner);
        writer.write(entity.health);
        writer.write(entity.x);
        writer.write(entity.y);
        writer.write(entity.vx);
        writer.write(entity.vy);
    }

    void writePayload(ByteWriter& writer, const Snapshot& packet) noexcept
    {
        writer.write(packet.serverTick);
        writer.write(packet.lastInputTick);
        writer.write(packet.partIndex);
        writer.write(packet.partCount);
        writer.write(static_cast<std::uint8_t>(packet.entities.size()));
        for (const EntityState& entity : packet.entities) {
            writeEntityState(writer, entity);
        }
    }

    void writePayload(ByteWriter& writer, const EntitySpawn& packet) noexcept
    {
        writer.write(packet.netId);
        writer.write(std::to_underlying(packet.kind));
        writer.write(packet.owner);
        writer.write(packet.x);
        writer.write(packet.y);
        writer.write(packet.vx);
        writer.write(packet.vy);
    }

    void writePayload(ByteWriter& writer, const EntityDestroy& packet) noexcept
    {
        writer.write(packet.netId);
        writer.write(std::to_underlying(packet.reason));
        writer.write(packet.killerNetId);
    }

    void writePayload(ByteWriter& writer, const PlayerJoined& packet) noexcept
    {
        writer.write(packet.playerId);
        writeName(writer, packet.name);
    }

    void writePayload(ByteWriter& writer, const PlayerLeft& packet) noexcept
    {
        writer.write(packet.playerId);
        writer.write(std::to_underlying(packet.reason));
    }

    void writePayload(ByteWriter& writer, const PlayerDied& packet) noexcept
    {
        writer.write(packet.playerId);
        writer.write(packet.killerNetId);
    }

    void writePayload(ByteWriter& writer, const GameState& packet) noexcept
    {
        writer.write(std::to_underlying(packet.phase));
        writer.write(packet.score);
    }

    // --- Payload readers ------------------------------------------------------
    //
    // A reader fills `packet` and returns an error only for what it can tell
    // by itself (bad name, count not matching the size). Truncation is caught
    // afterwards by `ByteReader::finished()`, ranges by `validate()`.

    using ReadError = std::optional<DecodeError>;

    ReadError readPayload(ByteReader& reader, ConnectRequest& packet)
    {
        return readName(reader, packet.name) ? ReadError{} : DecodeError::InvalidName;
    }

    ReadError readPayload(ByteReader& reader, ConnectAccept& packet)
    {
        packet.playerId = reader.read<std::uint8_t>();
        packet.tickRate = reader.read<std::uint8_t>();
        packet.snapshotRate = reader.read<std::uint8_t>();
        packet.maxPlayers = reader.read<std::uint8_t>();
        return {};
    }

    ReadError readPayload(ByteReader& reader, ConnectReject& packet)
    {
        packet.reason = static_cast<RejectReason>(reader.read<std::uint8_t>());
        return {};
    }

    ReadError readPayload(ByteReader& reader, Disconnect& packet)
    {
        packet.reason = static_cast<DisconnectReason>(reader.read<std::uint8_t>());
        return {};
    }

    ReadError readPayload(ByteReader& /*reader*/, Heartbeat& /*packet*/)
    {
        return {};
    }

    ReadError readPayload(ByteReader& reader, Input& packet)
    {
        packet.firstTick = reader.read<std::uint32_t>();
        packet.count = reader.read<std::uint8_t>();

        const auto count = static_cast<std::size_t>(packet.count);
        if (!reader.ok() || count < 1 || count > MaxInputMasks || reader.remaining() != count) {
            return DecodeError::BadPayloadSize;
        }
        for (std::size_t i = 0; i < count; ++i) {
            packet.masks[i] = reader.read<std::uint8_t>();
        }
        return {};
    }

    EntityState readEntityState(ByteReader& reader) noexcept
    {
        EntityState entity;
        entity.netId = reader.read<std::uint16_t>();
        entity.kind = static_cast<EntityKind>(reader.read<std::uint8_t>());
        entity.owner = reader.read<std::uint8_t>();
        entity.health = reader.read<std::uint8_t>();
        entity.x = reader.read<std::int16_t>();
        entity.y = reader.read<std::int16_t>();
        entity.vx = reader.read<std::int16_t>();
        entity.vy = reader.read<std::int16_t>();
        return entity;
    }

    ReadError readPayload(ByteReader& reader, Snapshot& packet)
    {
        packet.serverTick = reader.read<std::uint32_t>();
        packet.lastInputTick = reader.read<std::uint32_t>();
        packet.partIndex = reader.read<std::uint8_t>();
        packet.partCount = reader.read<std::uint8_t>();
        const auto count = static_cast<std::size_t>(reader.read<std::uint8_t>());

        // Checked BEFORE allocating: the count must match the bytes received.
        if (!reader.ok() || reader.remaining() != count * EntityStateSize) {
            return DecodeError::BadPayloadSize;
        }
        packet.entities.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            packet.entities.push_back(readEntityState(reader));
        }
        return {};
    }

    ReadError readPayload(ByteReader& reader, EntitySpawn& packet)
    {
        packet.netId = reader.read<std::uint16_t>();
        packet.kind = static_cast<EntityKind>(reader.read<std::uint8_t>());
        packet.owner = reader.read<std::uint8_t>();
        packet.x = reader.read<std::int16_t>();
        packet.y = reader.read<std::int16_t>();
        packet.vx = reader.read<std::int16_t>();
        packet.vy = reader.read<std::int16_t>();
        return {};
    }

    ReadError readPayload(ByteReader& reader, EntityDestroy& packet)
    {
        packet.netId = reader.read<std::uint16_t>();
        packet.reason = static_cast<DestroyReason>(reader.read<std::uint8_t>());
        packet.killerNetId = reader.read<std::uint16_t>();
        return {};
    }

    ReadError readPayload(ByteReader& reader, PlayerJoined& packet)
    {
        packet.playerId = reader.read<std::uint8_t>();
        return readName(reader, packet.name) ? ReadError{} : DecodeError::InvalidName;
    }

    ReadError readPayload(ByteReader& reader, PlayerLeft& packet)
    {
        packet.playerId = reader.read<std::uint8_t>();
        packet.reason = static_cast<LeaveReason>(reader.read<std::uint8_t>());
        return {};
    }

    ReadError readPayload(ByteReader& reader, PlayerDied& packet)
    {
        packet.playerId = reader.read<std::uint8_t>();
        packet.killerNetId = reader.read<std::uint16_t>();
        return {};
    }

    ReadError readPayload(ByteReader& reader, GameState& packet)
    {
        packet.phase = static_cast<GamePhase>(reader.read<std::uint8_t>());
        packet.score = reader.read<std::uint32_t>();
        return {};
    }

    // --- Decoding one type (§7 rules 4, 7, 8, 9) ------------------------------

    template <typename T>
    std::expected<Packet, DecodeError> decodeAs(
        const PacketHeader& header, std::span<const std::byte> payload, Direction direction)
    {
        if (T::Way != Direction::Both && T::Way != direction) {
            return std::unexpected(DecodeError::WrongDirection);
        }
        if (((header.flags & flags::Reliable) != 0) != T::Reliable) {
            return std::unexpected(DecodeError::ReliableFlagMismatch);
        }

        ByteReader reader(payload);
        Packet packet;
        packet.sessionToken = header.sessionToken;
        packet.sequence = header.sequence;
        if ((header.flags & flags::HasAck) != 0) {
            packet.ack = Ack{.sequence = header.ack, .bits = header.ackBits};
        }
        if constexpr (T::Reliable) {
            packet.reliableId = reader.read<std::uint16_t>();
        }

        T body{};
        if (const ReadError error = readPayload(reader, body)) {
            return std::unexpected(*error);
        }
        if (!reader.finished()) {
            return std::unexpected(DecodeError::BadPayloadSize);
        }
        if (!validate(body)) {
            return std::unexpected(DecodeError::InvalidValue);
        }

        packet.payload = std::move(body);
        return packet;
    }

} // namespace

// --- Public API ---------------------------------------------------------------

std::expected<std::size_t, EncodeError> encode(const Packet& packet, std::span<std::byte> output)
{
    return std::visit(
        [&]<typename T>(const T& body) -> std::expected<std::size_t, EncodeError> {
            if (!validate(body)) {
                return std::unexpected(EncodeError::InvalidPayload);
            }
            if (output.size() < HeaderSize) {
                return std::unexpected(EncodeError::BufferTooSmall);
            }

            ByteWriter payloadWriter(output.subspan(HeaderSize));
            if constexpr (T::Reliable) {
                payloadWriter.write(packet.reliableId);
            }
            writePayload(payloadWriter, body);
            if (!payloadWriter.ok()) {
                return std::unexpected(EncodeError::BufferTooSmall);
            }

            // validate() bounds every variable part, so the payload always
            // fits in MaxPayloadSize.
            const auto payloadSize = static_cast<std::uint16_t>(payloadWriter.size());
            const auto flagBits = static_cast<std::uint8_t>(
                (T::Reliable ? flags::Reliable : 0U) | (packet.ack ? flags::HasAck : 0U));
            const Ack ack = packet.ack.value_or(Ack{});

            ByteWriter headerWriter(output.first(HeaderSize));
            headerWriter.write(Magic);
            headerWriter.write(Version);
            headerWriter.write(std::to_underlying(T::Type));
            headerWriter.write(flagBits);
            headerWriter.write(std::uint8_t{0}); // reserved
            headerWriter.write(payloadSize);
            headerWriter.write(packet.sessionToken);
            headerWriter.write(packet.sequence);
            headerWriter.write(ack.sequence);
            headerWriter.write(ack.bits);

            return HeaderSize + payloadWriter.size();
        },
        packet.payload);
}

std::expected<PacketHeader, DecodeError> decodeHeader(std::span<const std::byte> datagram) noexcept
{
    if (datagram.size() < HeaderSize) {
        return std::unexpected(DecodeError::TooSmall);
    }
    if (datagram.size() > MaxDatagramSize) {
        return std::unexpected(DecodeError::TooLarge);
    }

    ByteReader reader(datagram);
    if (reader.read<std::uint16_t>() != Magic) {
        return std::unexpected(DecodeError::BadMagic);
    }

    PacketHeader header;
    header.version = reader.read<std::uint8_t>();
    header.type = reader.read<std::uint8_t>();
    header.flags = reader.read<std::uint8_t>();
    const auto reserved = reader.read<std::uint8_t>();
    header.payloadSize = reader.read<std::uint16_t>();
    header.sessionToken = reader.read<std::uint32_t>();
    header.sequence = reader.read<std::uint16_t>();
    header.ack = reader.read<std::uint16_t>();
    header.ackBits = reader.read<std::uint32_t>();

    if (reserved != 0 || (header.flags & ~unsigned{flags::Known}) != 0U) {
        return std::unexpected(DecodeError::ReservedBitsSet);
    }
    if (static_cast<std::size_t>(header.payloadSize) != datagram.size() - HeaderSize) {
        return std::unexpected(DecodeError::PayloadSizeMismatch);
    }
    return header;
}

std::expected<Packet, DecodeError> decode(std::span<const std::byte> datagram, Direction direction)
{
    const auto header = decodeHeader(datagram);
    if (!header) {
        return std::unexpected(header.error());
    }
    if (header->version != Version) {
        return std::unexpected(DecodeError::UnsupportedVersion);
    }

    const auto payload = datagram.subspan(HeaderSize);

    switch (static_cast<PacketType>(header->type)) {
    case PacketType::ConnectRequest:
        return decodeAs<ConnectRequest>(*header, payload, direction);
    case PacketType::ConnectAccept:
        return decodeAs<ConnectAccept>(*header, payload, direction);
    case PacketType::ConnectReject:
        return decodeAs<ConnectReject>(*header, payload, direction);
    case PacketType::Disconnect:
        return decodeAs<Disconnect>(*header, payload, direction);
    case PacketType::Heartbeat:
        return decodeAs<Heartbeat>(*header, payload, direction);
    case PacketType::Input:
        return decodeAs<Input>(*header, payload, direction);
    case PacketType::Snapshot:
        return decodeAs<Snapshot>(*header, payload, direction);
    case PacketType::EntitySpawn:
        return decodeAs<EntitySpawn>(*header, payload, direction);
    case PacketType::EntityDestroy:
        return decodeAs<EntityDestroy>(*header, payload, direction);
    case PacketType::PlayerJoined:
        return decodeAs<PlayerJoined>(*header, payload, direction);
    case PacketType::PlayerLeft:
        return decodeAs<PlayerLeft>(*header, payload, direction);
    case PacketType::PlayerDied:
        return decodeAs<PlayerDied>(*header, payload, direction);
    case PacketType::GameState:
        return decodeAs<GameState>(*header, payload, direction);
    }
    return std::unexpected(DecodeError::UnknownType);
}

bool isValidName(std::string_view name) noexcept
{
    return name.size() <= NameSize && !name.contains('\0')
        && isValidUtf8(name);
}

std::string_view toString(DecodeError error) noexcept
{
    switch (error) {
    case DecodeError::TooSmall:
        return "datagram smaller than the header";
    case DecodeError::TooLarge:
        return "datagram larger than the maximum size";
    case DecodeError::BadMagic:
        return "bad magic number";
    case DecodeError::ReservedBitsSet:
        return "reserved header bits set";
    case DecodeError::PayloadSizeMismatch:
        return "payload size does not match datagram size";
    case DecodeError::UnsupportedVersion:
        return "unsupported protocol version";
    case DecodeError::UnknownType:
        return "unknown packet type";
    case DecodeError::WrongDirection:
        return "packet type not allowed in this direction";
    case DecodeError::ReliableFlagMismatch:
        return "reliable flag inconsistent with packet type";
    case DecodeError::BadPayloadSize:
        return "bad payload size";
    case DecodeError::InvalidValue:
        return "field value out of range";
    case DecodeError::InvalidName:
        return "invalid player name";
    }
    return "unknown error";
}

std::string_view toString(EncodeError error) noexcept
{
    switch (error) {
    case EncodeError::InvalidPayload:
        return "payload breaks a protocol rule";
    case EncodeError::BufferTooSmall:
        return "output buffer too small";
    }
    return "unknown error";
}

} // namespace network::protocol
