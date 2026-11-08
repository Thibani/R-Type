#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

/// Constants and enumerations of the R-Type network protocol, version 9.
/// The reference is docs/protocol.md: section numbers below point to it.
namespace network::protocol {

// --- Sizes and identification (§2, §3) ---------------------------------------

inline constexpr std::uint16_t Magic = 0x5254; // "RT"
inline constexpr std::uint8_t Version = 9;

inline constexpr std::size_t HeaderSize = 20;
inline constexpr std::size_t MaxDatagramSize = 1200;
inline constexpr std::size_t MaxPayloadSize = MaxDatagramSize - HeaderSize;
inline constexpr std::size_t SealOverhead = 24; // Added by encryption (§3.3): counter + tag

inline constexpr std::size_t NameSize = 16;
inline constexpr std::size_t MinPasswordSize = 8; // Only checked when an account is created
inline constexpr std::size_t MaxPasswordSize = 64;
inline constexpr std::size_t MaxChatSize = 200; // Bytes of UTF-8 in one message
inline constexpr std::size_t GroupCodeSize = 6;
// 32 signs: neither I, O, 0 nor 1, which are easy to mistake when reading a code aloud
inline constexpr std::string_view GroupCodeAlphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
inline constexpr std::size_t MaxInputMasks = 8;
inline constexpr std::size_t SnapshotHeaderSize = 11;
inline constexpr std::size_t EntityStateSize = 13;
inline constexpr std::size_t MaxSnapshotEntities
    = (MaxPayloadSize - SealOverhead - SnapshotHeaderSize) / EntityStateSize; // 88

inline constexpr std::uint8_t MaxPlayers = 4;
inline constexpr std::uint8_t NoPlayer = 0xFF;
inline constexpr std::uint16_t NoEntity = 0xFFFF;
/// `GAME_STATE.bossProgress` is a percentage.
inline constexpr std::uint8_t MaxBossProgress = 100;

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
    AuthRequest = 0x06,
    AuthResult = 0x07,
    Input = 0x10,
    Restart = 0x11,
    Snapshot = 0x20,
    EntitySpawn = 0x21,
    EntityDestroy = 0x22,
    PlayerJoined = 0x23,
    PlayerLeft = 0x24,
    PlayerDied = 0x25,
    GameState = 0x26,
    JoinGame = 0x30,
    LeaveGame = 0x31,
    JoinResult = 0x32,
    FriendRequest = 0x33,
    FriendAnswer = 0x34,
    FriendRemove = 0x35,
    FriendResult = 0x36,
    FriendEntry = 0x37,
    ChatSend = 0x38,
    PrivateSend = 0x39,
    ChatMessage = 0x3A,
    ChatRefused = 0x3B,
    GroupCode = 0x3C,
    GroupJoin = 0x3D,
    GroupLeave = 0x3E,
    GroupKick = 0x3F,
    GroupPromote = 0x40,
    GroupInvite = 0x41,
    GroupAnswer = 0x42,
    GroupReady = 0x43,
    GroupState = 0x44,
    GroupResult = 0x45,
    GroupInvited = 0x46,
    LeaderboardRequest = 0x47,
    Leaderboard = 0x48,
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

enum class AuthMode : std::uint8_t {
    Login = 0,
    Register = 1,
};

enum class AuthCode : std::uint8_t {
    Ok = 0,
    WrongCredentials = 1, // Same answer for an unknown name and a wrong password
    NameTaken = 2,
    AlreadyConnected = 3,
    InvalidPassword = 4,
    InvalidName = 5,
    ServerError = 6,
};

enum class JoinCode : std::uint8_t {
    Ok = 0,
    NotLeader = 1, // Only the leader starts the game
    NotReady = 2, // A member is not ready, or still in a game
};

/// Bits of a member, in GROUP_STATE.
namespace groupMember {
inline constexpr std::uint8_t Ready = 1U << 0U;
inline constexpr std::uint8_t InGame = 1U << 1U;
inline constexpr std::uint8_t Known = Ready | InGame;
} // namespace groupMember

inline constexpr std::size_t GroupMemberSize = NameSize + 1; // In GROUP_STATE

enum class GroupCodeAction : std::uint8_t {
    Generate = 0, // A new code replaces the old one
    Delete = 1,
};

enum class GroupOutcome : std::uint8_t {
    Invited = 0, // The invitation went out
    UnknownCode = 1,
    GroupFull = 2,
    Kicked = 3, // This group's code no longer works for this player
    GroupInGame = 4,
    NotLeader = 5,
    NotMember = 6,
    Alone = 7, // Nobody to leave
    TooManyTries = 8, // Too many wrong codes: wait
    NotAvailable = 9, // Invited: not a friend, or offline
    InvitationGone = 10,
    RemovedFromGroup = 11, // The leader removed this player
};

/// The two rankings: games started alone, and games started by a group of 2 to 4.
enum class Board : std::uint8_t {
    Solo = 0,
    Multi = 1,
};

inline constexpr std::size_t LeaderboardSize = 10; // Best games shown
// One line of LEADERBOARD: the score, then MaxPlayers names (unused ones are zero bytes)
inline constexpr std::size_t LeaderboardEntrySize = 4 + (MaxPlayers * NameSize);

enum class FriendReply : std::uint8_t {
    Refuse = 0,
    Accept = 1,
};

enum class FriendCode : std::uint8_t {
    Ok = 0,
    UnknownName = 1, // No account has this name
    AlreadyLinked = 2, // Already a friend, or already asked
    Yourself = 3,
    LimitReached = 4, // 50 friends, or 50 requests
};

/// What someone is to the player receiving FRIEND_ENTRY.
enum class FriendState : std::uint8_t {
    Offline = 0,
    Online = 1,
    RequestReceived = 2, // They asked the player
    RequestSent = 3, // The player asked them
    Removed = 4, // Nothing any more: forget them
};

/// Who a CHAT_MESSAGE is for, and what its `name` means.
enum class ChatKind : std::uint8_t {
    Game = 0, // For the group, in a game or not; `name` wrote it
    Received = 1, // Private; `name` wrote it
    Sent = 2, // Private, our own, as the server took it; `name` receives it
};

enum class ChatRefusal : std::uint8_t {
    TooFast = 0,
    NotFriend = 1,
    MailboxFull = 2,
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

/// Kind of a networked entity. Values 16-255 are reserved for new content:
/// receivers keep unknown kinds as-is instead of rejecting the packet.
enum class EntityKind : std::uint8_t {
    PlayerShip = 0,
    Bydo = 1,
    PlayerMissile = 2,
    EnemyMissile = 3,
    PataPata = 4,
    Bug = 5,
    Cancer = 6,
    Cruiser = 7,
    Hopper = 8,
    Spinner = 9,
    Gunner = 10,
    Mine = 11,
    Boss = 12,
    Force = 13, // The pod attached to a ship; its owner is the ship's player
    ForceCapsule = 14, // Pickup: gives a Force
    RepairKit = 15, // Pickup: repairs a ship
};

/// Bits of an input mask (§4.2).
namespace input {
    inline constexpr std::uint8_t Up = 1U << 0U;
    inline constexpr std::uint8_t Down = 1U << 1U;
    inline constexpr std::uint8_t Left = 1U << 2U;
    inline constexpr std::uint8_t Right = 1U << 3U;
    inline constexpr std::uint8_t Fire = 1U << 4U;
    inline constexpr std::uint8_t Pod = 1U << 5U; // Moves the Force to its next slot
    inline constexpr std::uint8_t Known = 0x3FU; // Up | Down | Left | Right | Fire | Pod
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
