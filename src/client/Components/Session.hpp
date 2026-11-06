#pragma once

#include "client/Components/Settings.hpp"
#include "network/Protocol/Protocol.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

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
    bool joining{false}; // JOIN_GAME sent, JOIN_RESULT not received yet

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

} // namespace client
