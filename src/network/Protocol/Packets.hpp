#pragma once

#include "network/Protocol/Protocol.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

/// In-memory form of every packet of docs/protocol.md (§4).
///
/// These structs are plain data: they are never copied to the network as-is.
/// `PacketCodec` serializes them field by field.
///
/// Each payload declares three traits used by the codec:
///  - `Type`: its packet type;
///  - `Way`: which way it may travel;
///  - `Reliable`: whether it is sent with the RELIABLE flag (and a reliableId).
namespace network::protocol {

// --- Connection (§4.1) -------------------------------------------------------

struct ConnectRequest {
    static constexpr PacketType Type = PacketType::ConnectRequest;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = false;

    std::string name; // At most 16 bytes of valid UTF-8, no '\0'

    friend bool operator==(const ConnectRequest&, const ConnectRequest&) = default;
};

struct ConnectAccept {
    static constexpr PacketType Type = PacketType::ConnectAccept;
    static constexpr Direction Way = Direction::ServerToClient;
    static constexpr bool Reliable = false;

    std::uint8_t playerId{};
    std::uint8_t tickRate{};
    std::uint8_t snapshotRate{};
    std::uint8_t maxPlayers{};

    friend bool operator==(const ConnectAccept&, const ConnectAccept&) = default;
};

struct ConnectReject {
    static constexpr PacketType Type = PacketType::ConnectReject;
    static constexpr Direction Way = Direction::ServerToClient;
    static constexpr bool Reliable = false;

    RejectReason reason{RejectReason::ServerFull};

    friend bool operator==(const ConnectReject&, const ConnectReject&) = default;
};

struct Disconnect {
    static constexpr PacketType Type = PacketType::Disconnect;
    static constexpr Direction Way = Direction::Both;
    static constexpr bool Reliable = false;

    DisconnectReason reason{DisconnectReason::Quit};

    friend bool operator==(const Disconnect&, const Disconnect&) = default;
};

struct Heartbeat {
    static constexpr PacketType Type = PacketType::Heartbeat;
    static constexpr Direction Way = Direction::Both;
    static constexpr bool Reliable = false;

    friend bool operator==(const Heartbeat&, const Heartbeat&) = default;
};

// --- Client -> server (§4.2) -------------------------------------------------

/// The last `count` input masks of the client, oldest first.
/// `masks[i]` is the input of tick `firstTick + i`.
struct Input {
    static constexpr PacketType Type = PacketType::Input;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = false;

    std::uint32_t firstTick{};
    std::uint8_t count{}; // 1 to MaxInputMasks
    std::array<std::uint8_t, MaxInputMasks> masks{};

    friend bool operator==(const Input&, const Input&) = default;
};

// --- Server -> client (§4.3) -------------------------------------------------

/// State of one entity inside a snapshot. Positions and velocities are
/// already quantized: use `quantize()` / `dequantize()`.
struct EntityState {
    std::uint16_t netId{};
    EntityKind kind{EntityKind::PlayerShip};
    std::uint8_t owner{NoPlayer};
    std::uint8_t health{};
    std::int16_t x{};
    std::int16_t y{};
    std::int16_t vx{};
    std::int16_t vy{};

    friend bool operator==(const EntityState&, const EntityState&) = default;
};

struct Snapshot {
    static constexpr PacketType Type = PacketType::Snapshot;
    static constexpr Direction Way = Direction::ServerToClient;
    static constexpr bool Reliable = false;

    std::uint32_t serverTick{};
    std::uint32_t lastInputTick{};
    std::uint8_t partIndex{};
    std::uint8_t partCount{1};
    std::vector<EntityState> entities; // At most MaxSnapshotEntities

    friend bool operator==(const Snapshot&, const Snapshot&) = default;
};

struct EntitySpawn {
    static constexpr PacketType Type = PacketType::EntitySpawn;
    static constexpr Direction Way = Direction::ServerToClient;
    static constexpr bool Reliable = true;

    std::uint16_t netId{};
    EntityKind kind{EntityKind::PlayerShip};
    std::uint8_t owner{NoPlayer};
    std::int16_t x{};
    std::int16_t y{};
    std::int16_t vx{};
    std::int16_t vy{};

    friend bool operator==(const EntitySpawn&, const EntitySpawn&) = default;
};

struct EntityDestroy {
    static constexpr PacketType Type = PacketType::EntityDestroy;
    static constexpr Direction Way = Direction::ServerToClient;
    static constexpr bool Reliable = true;

    std::uint16_t netId{};
    DestroyReason reason{DestroyReason::Killed};
    std::uint16_t killerNetId{NoEntity};

    friend bool operator==(const EntityDestroy&, const EntityDestroy&) = default;
};

struct PlayerJoined {
    static constexpr PacketType Type = PacketType::PlayerJoined;
    static constexpr Direction Way = Direction::ServerToClient;
    static constexpr bool Reliable = true;

    std::uint8_t playerId{};
    std::string name;

    friend bool operator==(const PlayerJoined&, const PlayerJoined&) = default;
};

struct PlayerLeft {
    static constexpr PacketType Type = PacketType::PlayerLeft;
    static constexpr Direction Way = Direction::ServerToClient;
    static constexpr bool Reliable = true;

    std::uint8_t playerId{};
    LeaveReason reason{LeaveReason::Quit};

    friend bool operator==(const PlayerLeft&, const PlayerLeft&) = default;
};

struct PlayerDied {
    static constexpr PacketType Type = PacketType::PlayerDied;
    static constexpr Direction Way = Direction::ServerToClient;
    static constexpr bool Reliable = true;

    std::uint8_t playerId{};
    std::uint16_t killerNetId{NoEntity};

    friend bool operator==(const PlayerDied&, const PlayerDied&) = default;
};

struct GameState {
    static constexpr PacketType Type = PacketType::GameState;
    static constexpr Direction Way = Direction::ServerToClient;
    static constexpr bool Reliable = true;

    GamePhase phase{GamePhase::WaitingForPlayers};
    std::uint32_t score{};

    friend bool operator==(const GameState&, const GameState&) = default;
};

// --- Whole packet ------------------------------------------------------------

using Payload = std::variant<ConnectRequest, ConnectAccept, ConnectReject, Disconnect, Heartbeat,
    Input, Snapshot, EntitySpawn, EntityDestroy, PlayerJoined, PlayerLeft, PlayerDied, GameState>;

/// Acknowledgement carried by a packet (HAS_ACK flag, §6.1).
struct Ack {
    std::uint16_t sequence{}; // Newest sequence received from the peer
    std::uint32_t bits{}; // Bit i set <=> sequence - 1 - i was received

    friend bool operator==(const Ack&, const Ack&) = default;
};

/// A packet as handled by the rest of the program.
///
/// `type`, `payloadSize` and the RELIABLE flag of the header are not stored:
/// the codec derives them from the payload.
struct Packet {
    std::uint32_t sessionToken{};
    std::uint16_t sequence{};
    std::optional<Ack> ack;
    std::uint16_t reliableId{}; // Only meaningful when the payload is Reliable
    Payload payload;

    friend bool operator==(const Packet&, const Packet&) = default;
};

/// Header fields as read from the wire, before the payload is decoded.
struct PacketHeader {
    std::uint8_t version{};
    std::uint8_t type{}; // Raw: may be an unknown type
    std::uint8_t flags{};
    std::uint16_t payloadSize{};
    std::uint32_t sessionToken{};
    std::uint16_t sequence{};
    std::uint16_t ack{};
    std::uint32_t ackBits{};
};

/// A buffer large enough for any datagram of the protocol.
using DatagramBuffer = std::array<std::byte, MaxDatagramSize>;

} // namespace network::protocol
