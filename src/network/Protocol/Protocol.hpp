#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

/// Constants and enumerations of the R-Type network protocol, version 1.
/// The reference is docs/protocol.md: section numbers below point to it.
namespace network::protocol {

// --- Sizes and identification (§2, §3) ---------------------------------------

inline constexpr std::uint16_t Magic = 0x5254; // "RT"
inline constexpr std::uint8_t Version = 1;

inline constexpr std::size_t HeaderSize = 20;
inline constexpr std::size_t MaxDatagramSize = 1200;
inline constexpr std::size_t MaxPayloadSize = MaxDatagramSize - HeaderSize;

inline constexpr std::size_t NameSize = 16;
inline constexpr std::size_t MaxInputMasks = 8;
inline constexpr std::size_t SnapshotHeaderSize = 11;
inline constexpr std::size_t EntityStateSize = 13;
inline constexpr std::size_t MaxSnapshotEntities
    = (MaxPayloadSize - SnapshotHeaderSize) / EntityStateSize; // 89

inline constexpr std::uint8_t MaxPlayers = 4;
inline constexpr std::uint8_t NoPlayer = 0xFF;
inline constexpr std::uint16_t NoEntity = 0xFFFF;

// --- Header flags (§3.1) -----------------------------------------------------

namespace flags {
    inline constexpr std::uint8_t Reliable = 1U << 0U;
    inline constexpr std::uint8_t HasAck = 1U << 1U;
    inline constexpr std::uint8_t Known = 0x03U; // Reliable | HasAck
} // namespace flags

// --- Packet types (§4) -------------------------------------------------------

enum class PacketType : std::uint8_t {
    ConnectRequest = 0x01,
    ConnectAccept = 0x02,
    ConnectReject = 0x03,
    Disconnect = 0x04,
    Heartbeat = 0x05,
    Input = 0x10,
    Snapshot = 0x20,
    EntitySpawn = 0x21,
    EntityDestroy = 0x22,
    PlayerJoined = 0x23,
    PlayerLeft = 0x24,
    PlayerDied = 0x25,
    GameState = 0x26,
};

/// Which way a packet travels. A receiver drops packets going the wrong way.
enum class Direction : std::uint8_t {
    ClientToServer,
    ServerToClient,
    Both,
};

// --- Enumerations carried in payloads (§4, §5) -------------------------------

enum class RejectReason : std::uint8_t {
    ServerFull = 1,
    VersionMismatch = 2,
    GameInProgress = 3,
    InvalidName = 4,
};

enum class DisconnectReason : std::uint8_t {
    Quit = 0,
    Timeout = 1,
    Kicked = 2,
    ServerShutdown = 3,
    ProtocolError = 4,
};

enum class LeaveReason : std::uint8_t {
    Quit = 0,
    Timeout = 1,
    Kicked = 2,
};

enum class DestroyReason : std::uint8_t {
    Killed = 0,
    OutOfBounds = 1,
    Expired = 2,
};

enum class GamePhase : std::uint8_t {
    WaitingForPlayers = 0,
    Running = 1,
    GameOver = 2,
};

/// Kind of a networked entity. Values 4-255 are reserved for new content:
/// receivers keep unknown kinds as-is instead of rejecting the packet.
enum class EntityKind : std::uint8_t {
    PlayerShip = 0,
    Bydo = 1,
    PlayerMissile = 2,
    EnemyMissile = 3,
};

/// Bits of an input mask (§4.2).
namespace input {
    inline constexpr std::uint8_t Up = 1U << 0U;
    inline constexpr std::uint8_t Down = 1U << 1U;
    inline constexpr std::uint8_t Left = 1U << 2U;
    inline constexpr std::uint8_t Right = 1U << 3U;
    inline constexpr std::uint8_t Fire = 1U << 4U;
    inline constexpr std::uint8_t Known = 0x1FU; // Up | Down | Left | Right | Fire
} // namespace input

// --- Quantization (§2.1) -----------------------------------------------------

/// Number of wire steps per world unit: positions are sent in 1/4 units.
inline constexpr float QuantizationScale = 4.0F;

/// Converts a position or velocity to its wire value, clamped to `i16`.
[[nodiscard]]
inline std::int16_t quantize(float value) noexcept
{
    constexpr float min = std::numeric_limits<std::int16_t>::min();
    constexpr float max = std::numeric_limits<std::int16_t>::max();

    if (std::isnan(value)) {
        return 0;
    }
    return static_cast<std::int16_t>(std::clamp(std::round(value * QuantizationScale), min, max));
}

/// Converts a wire value back to a position or velocity.
[[nodiscard]]
constexpr float dequantize(std::int16_t value) noexcept
{
    return static_cast<float>(value) / QuantizationScale;
}

} // namespace network::protocol
