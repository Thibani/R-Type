#pragma once

#include "client/Components/Settings.hpp"
#include "network/Protocol/Packets.hpp"
#include "network/Protocol/Protocol.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace client {

enum class Screen : std::uint8_t {
    Connect, // Connection form (also shows "connecting" and errors)
    Home, // Main page, once logged in
    WaitingRoom, // GAME_STATE = WAITING_FOR_PLAYERS
    Game, // Playing (JOIN_RESULT accepted, or GAME_STATE = RUNNING)
    GameOver, // GAME_STATE = GAME_OVER
};

/// What the player asked for in the interface, done by the network systems.
enum class Request : std::uint8_t {
    None,
    Login,
    CreateAccount,
    Leave, // Log out: back to the connection screen
    Play, // Enter the game
    LeaveGame, // Back to the main page
    Quit, // Close the client
};

/// A player as told by PLAYER_JOINED / PLAYER_LEFT / PLAYER_DIED.
struct PlayerSlot {
    bool present{false};
    bool left{false}; // Shown as "Disconnected" until the slot is reused
    bool down{false}; // PLAYER_DIED
    std::string name;
};

/// One line of the friends panel, as told by FRIEND_ENTRY.
struct FriendRow {
    std::string name;
    network::protocol::FriendState state{network::protocol::FriendState::Offline};
};

/// One line of the chat. `Notice`s come from the client itself; the others from CHAT_MESSAGE.
struct ChatLine {
    enum class Kind : std::uint8_t { Game, Received, Sent, Notice };
    Kind kind{Kind::Notice};
    std::string name; // Who wrote it, or who receives it if `Sent`
    std::string text;
};

/// Singleton: everything the interface shows that is not an entity.
struct Session {
    Screen screen{Screen::Connect};
    Request request{Request::None};
    int focus{0}; // Focused field or button of the current screen
    bool menuOpen{false}; // Esc menu over the game
    bool settingsOpen{false}; // Settings over the current screen (connection form or game)
    std::optional<Action> capturing; // Settings: waiting for the key to bind to this action

    // Connection form
    std::string name;
    std::string password; // Forgotten once logged in
    std::string address;
    std::string port;
    std::string status; // "Connecting... attempt 3 / 10", "Logging in..."
    std::string error; // Why the last attempt or session failed, or why the game refused us

    // Main page
    network::protocol::GroupState group; // Empty until the first GROUP_STATE
    std::string groupCodeInput; // What is typed in "Join with a code"
    std::vector<std::string> invitations; // Who invited us, oldest first

    // Friends panel, on the main page. The list stays up to date during a game
    std::vector<FriendRow> friends; // Requests received, online, offline, requests sent
    std::string friendName; // What is typed in the panel
    std::string friendMessage; // Answer to the last request
    std::size_t friendScroll{0}; // First row shown
    bool friendsHovered{false}; // The mouse is over the bar
    std::optional<std::string> friendMenu; // The friend whose right-click menu is open
    std::vector<network::protocol::Payload> outgoing; // Reliable, sent by NetSendSystem

    // Chat, on the main page and in the game
    std::vector<ChatLine> chat; // Oldest first, MaxChatLines at most
    std::size_t chatScroll{0}; // Newest messages hidden by scrolling back
    bool chatOpen{false}; // Typing: every key goes to the chat, none to the ship
    std::string chatInput;
    std::optional<std::string> chatTarget; // "To [name]:", from the right-click menu

    // Leaderboard, over the main page
    bool leaderboardOpen{false};
    network::protocol::Board board{network::protocol::Board::Solo}; // The tab shown
    std::optional<network::protocol::Leaderboard> leaderboard; // Last answer for `board`

    // Game
    std::optional<std::uint8_t> playerId; // Ours, from JOIN_RESULT: only while in the game
    std::array<PlayerSlot, network::protocol::MaxPlayers> players{};
    // From JOIN_RESULT to the next reliable event: the PLAYER_JOINED received
    // meanwhile are the players already there (§8.1), not arrivals to announce
    bool catchingUp{false};
    std::uint32_t score{0}; // Team score from GAME_STATE
    std::string notice; // Short message ("P4 Dave left the game"), shown until noticeUntil
    float noticeUntil{0.0F};
};

/// True if we lead our group. Alone and not told yet, we do.
[[nodiscard]]
inline bool leadsTheGroup(const Session& session)
{
    return session.group.members.empty()
        || session.group.members.at(session.group.leader).name == session.name;
}

} // namespace client
