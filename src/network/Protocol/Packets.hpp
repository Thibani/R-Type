#pragma once

#include "network/Crypto/Crypto.hpp"
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
    crypto::PublicKey publicKey{}; // The client's half of the key exchange (§3.3)

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
    crypto::PublicKey publicKey{}; // The server's half of the key exchange (§3.3)

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

/// Logs in to an account or creates one, for the name of CONNECT_REQUEST.
/// Always sealed. Sent again every 500 ms until AUTH_RESULT arrives.
struct AuthRequest {
    static constexpr PacketType Type = PacketType::AuthRequest;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = false;

    AuthMode mode{AuthMode::Login};
    std::string password; // At most 64 bytes of valid UTF-8, no '\0'

    friend bool operator==(const AuthRequest&, const AuthRequest&) = default;
};

/// Reliable, and the session's first reliable message: the events that follow
/// a successful login can never be delivered before it.
struct AuthResult {
    static constexpr PacketType Type = PacketType::AuthResult;
    static constexpr Direction Way = Direction::ServerToClient;
    static constexpr bool Reliable = true;

    AuthCode code{AuthCode::Ok};

    friend bool operator==(const AuthResult&, const AuthResult&) = default;
};

// --- Client -> server (§4.2) -------------------------------------------------

/// After GAME_OVER: start a new game. Sent a few times, as it is unreliable;
/// the server ignores it while a game is running.
struct Restart {
    static constexpr PacketType Type = PacketType::Restart;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = false;

    friend bool operator==(const Restart&, const Restart&) = default;
};

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

/// The player asks to enter the game. Answered by JOIN_RESULT.
struct JoinGame { // The leader starts the group's game
    static constexpr PacketType Type = PacketType::JoinGame;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = true;

    friend bool operator==(const JoinGame&, const JoinGame&) = default;
};

/// The player goes back to the main page, and stays connected.
struct LeaveGame {
    static constexpr PacketType Type = PacketType::LeaveGame;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = true;

    friend bool operator==(const LeaveGame&, const LeaveGame&) = default;
};

/// Asks an account, by its exact name, to become a friend. Answered by FRIEND_RESULT.
struct FriendRequest {
    static constexpr PacketType Type = PacketType::FriendRequest;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = true;

    std::string name;

    friend bool operator==(const FriendRequest&, const FriendRequest&) = default;
};

/// Accepts or refuses the request received from `name`.
struct FriendAnswer {
    static constexpr PacketType Type = PacketType::FriendAnswer;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = true;

    std::string name;
    FriendReply reply{FriendReply::Refuse};

    friend bool operator==(const FriendAnswer&, const FriendAnswer&) = default;
};

/// Removes a friend, or cancels the request sent to `name`.
struct FriendRemove {
    static constexpr PacketType Type = PacketType::FriendRemove;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = true;

    std::string name;

    friend bool operator==(const FriendRemove&, const FriendRemove&) = default;
};

/// A message for the players of the game.
struct ChatSend {
    static constexpr PacketType Type = PacketType::ChatSend;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = true;

    std::string text; // 1 to MaxChatSize bytes of valid UTF-8, no control character

    friend bool operator==(const ChatSend&, const ChatSend&) = default;
};

/// A private message for a friend, online or not.
struct PrivateSend {
    static constexpr PacketType Type = PacketType::PrivateSend;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = true;

    std::string name;
    std::string text;

    friend bool operator==(const PrivateSend&, const PrivateSend&) = default;
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
    std::uint8_t level{1}; // Endless mode level, 1 at the start of a game
    std::uint8_t lives{}; // Ships the team can still lose
    std::uint8_t bossProgress{}; // Distance to the next boss, 0-100 %; 100 while it is there

    friend bool operator==(const GameState&, const GameState&) = default;
};

/// Answer to JOIN_GAME. Reliable, and sent before any event of the game.
struct JoinResult {
    static constexpr PacketType Type = PacketType::JoinResult;
    static constexpr Direction Way = Direction::ServerToClient;
    static constexpr bool Reliable = true;

    JoinCode code{JoinCode::Ok};
    std::uint8_t playerId{NoPlayer}; // 0..3 if `Ok`, NoPlayer otherwise

    friend bool operator==(const JoinResult&, const JoinResult&) = default;
};

/// The leader asks for a code, or deletes it.
struct GroupCode {
    static constexpr PacketType Type = PacketType::GroupCode;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = true;

    GroupCodeAction action{GroupCodeAction::Generate};

    friend bool operator==(const GroupCode&, const GroupCode&) = default;
};

/// Joins the group that has this code.
struct GroupJoin {
    static constexpr PacketType Type = PacketType::GroupJoin;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = true;

    std::string code; // GroupCodeSize characters of the code's alphabet

    friend bool operator==(const GroupJoin&, const GroupJoin&) = default;
};

/// Leaves the group for a new one, alone.
struct GroupLeave {
    static constexpr PacketType Type = PacketType::GroupLeave;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = true;

    friend bool operator==(const GroupLeave&, const GroupLeave&) = default;
};

/// The leader removes a member.
struct GroupKick {
    static constexpr PacketType Type = PacketType::GroupKick;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = true;

    std::string name; // 1 to 16 bytes

    friend bool operator==(const GroupKick&, const GroupKick&) = default;
};

/// The leader gives the role to a member.
struct GroupPromote {
    static constexpr PacketType Type = PacketType::GroupPromote;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = true;

    std::string name; // 1 to 16 bytes

    friend bool operator==(const GroupPromote&, const GroupPromote&) = default;
};

/// The leader invites a friend online.
struct GroupInvite {
    static constexpr PacketType Type = PacketType::GroupInvite;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = true;

    std::string name; // 1 to 16 bytes

    friend bool operator==(const GroupInvite&, const GroupInvite&) = default;
};

/// The answer to GROUP_INVITED.
struct GroupAnswer {
    static constexpr PacketType Type = PacketType::GroupAnswer;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = true;

    std::string name; // Who invited: 1 to 16 bytes
    FriendReply reply{FriendReply::Refuse};

    friend bool operator==(const GroupAnswer&, const GroupAnswer&) = default;
};

/// A member says whether they are ready to play.
struct GroupReady {
    static constexpr PacketType Type = PacketType::GroupReady;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = true;

    bool ready{false};

    friend bool operator==(const GroupReady&, const GroupReady&) = default;
};

/// One member, in GROUP_STATE.
struct GroupMemberState {
    std::string name;
    bool ready{false};
    bool inGame{false};

    friend bool operator==(const GroupMemberState&, const GroupMemberState&) = default;
};

/// The whole group, sent whenever something in it changes.
struct GroupState {
    static constexpr PacketType Type = PacketType::GroupState;
    static constexpr Direction Way = Direction::ServerToClient;
    static constexpr bool Reliable = true;

    std::string code; // Empty: no code
    std::uint8_t countdown{0}; // Seconds left before the game starts; 0: none
    std::uint8_t leader{0}; // Index in `members`
    std::vector<GroupMemberState> members; // 1 to MaxPlayers

    friend bool operator==(const GroupState&, const GroupState&) = default;
};

/// What came of a group order, when it needs saying.
struct GroupResult {
    static constexpr PacketType Type = PacketType::GroupResult;
    static constexpr Direction Way = Direction::ServerToClient;
    static constexpr bool Reliable = true;

    GroupOutcome outcome{GroupOutcome::Invited};

    friend bool operator==(const GroupResult&, const GroupResult&) = default;
};

/// A leader invites this player into their group.
struct GroupInvited {
    static constexpr PacketType Type = PacketType::GroupInvited;
    static constexpr Direction Way = Direction::ServerToClient;
    static constexpr bool Reliable = true;

    std::string name; // Who invited: 1 to 16 bytes

    friend bool operator==(const GroupInvited&, const GroupInvited&) = default;
};

/// Answer to FRIEND_REQUEST.
struct FriendResult {
    static constexpr PacketType Type = PacketType::FriendResult;
    static constexpr Direction Way = Direction::ServerToClient;
    static constexpr bool Reliable = true;

    FriendCode code{FriendCode::Ok};

    friend bool operator==(const FriendResult&, const FriendResult&) = default;
};

/// One line of the player's friends list, new or changed.
struct FriendEntry {
    static constexpr PacketType Type = PacketType::FriendEntry;
    static constexpr Direction Way = Direction::ServerToClient;
    static constexpr bool Reliable = true;

    std::string name;
    FriendState state{FriendState::Offline};

    friend bool operator==(const FriendEntry&, const FriendEntry&) = default;
};

/// One line of chat. The server fills `name`: a client cannot write as someone else.
struct ChatMessage {
    static constexpr PacketType Type = PacketType::ChatMessage;
    static constexpr Direction Way = Direction::ServerToClient;
    static constexpr bool Reliable = true;

    ChatKind kind{ChatKind::Game};
    std::string name;
    std::string text;

    friend bool operator==(const ChatMessage&, const ChatMessage&) = default;
};

struct ChatRefused {
    static constexpr PacketType Type = PacketType::ChatRefused;
    static constexpr Direction Way = Direction::ServerToClient;
    static constexpr bool Reliable = true;

    ChatRefusal reason{ChatRefusal::TooFast};

    friend bool operator==(const ChatRefused&, const ChatRefused&) = default;
};

/// The player asks for one of the two rankings.
struct LeaderboardRequest {
    static constexpr PacketType Type = PacketType::LeaderboardRequest;
    static constexpr Direction Way = Direction::ClientToServer;
    static constexpr bool Reliable = true;

    Board board{Board::Solo};

    friend bool operator==(const LeaderboardRequest&, const LeaderboardRequest&) = default;
};

/// One game of a ranking: its score, and the accounts that started it.
struct LeaderboardEntry {
    std::uint32_t score{0};
    std::vector<std::string> names; // 1 to MaxPlayers

    friend bool operator==(const LeaderboardEntry&, const LeaderboardEntry&) = default;
};

/// Answer to LEADERBOARD_REQUEST: the best games, then the player's last game.
struct Leaderboard {
    static constexpr PacketType Type = PacketType::Leaderboard;
    static constexpr Direction Way = Direction::ServerToClient;
    static constexpr bool Reliable = true;

    Board board{Board::Solo};
    std::vector<LeaderboardEntry> best; // 0 to LeaderboardSize, best first
    std::uint32_t lastRank{0}; // Rank of the player's last game in this ranking; 0: none
    std::uint32_t lastScore{0};

    friend bool operator==(const Leaderboard&, const Leaderboard&) = default;
};

// --- Whole packet ------------------------------------------------------------

using Payload = std::variant<ConnectRequest, ConnectAccept, ConnectReject, Disconnect, Heartbeat,
    AuthRequest, AuthResult, Input, JoinGame, LeaveGame, FriendRequest, FriendAnswer, FriendRemove,
    ChatSend, PrivateSend, Snapshot, EntitySpawn, EntityDestroy, PlayerJoined, PlayerLeft,
    PlayerDied, GameState, JoinResult, FriendResult, FriendEntry, ChatMessage, ChatRefused,
    GroupCode, GroupJoin, GroupLeave, GroupKick, GroupPromote, GroupInvite, GroupAnswer, GroupReady,
    GroupState, GroupResult, GroupInvited, LeaderboardRequest, Leaderboard, Restart>;

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
