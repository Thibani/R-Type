#pragma once

#include "network/Protocol/Protocol.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace client {

enum class Screen : std::uint8_t {
    Connect, // Connection form (also shows "connecting" and errors)
    WaitingRoom, // GAME_STATE = WAITING_FOR_PLAYERS
    Game, // Playing (the server spawned our ship, or GAME_STATE = RUNNING)
    GameOver, // GAME_STATE = GAME_OVER
};

/// What the player asked for in the interface, done by the network systems.
enum class Request : std::uint8_t {
    None,
    Connect,
    Leave, // Back to the connection screen
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

    // Connection form
    std::string name;
    std::string address;
    std::string port;
    std::string status; // "Connecting... attempt 3 / 10"
    std::string error; // Why the last attempt or session failed

    // Game
    std::optional<std::uint8_t> playerId; // Ours, from CONNECT_ACCEPT
    std::array<PlayerSlot, network::protocol::MaxPlayers> players{};
    // From CONNECT_ACCEPT to the next reliable event: the PLAYER_JOINED received
    // meanwhile are the players already there (§8.1), not arrivals to announce
    bool catchingUp{false};
    std::uint32_t score{0}; // Team score from GAME_STATE
    std::string notice; // Short message ("P4 Dave left the game"), shown until noticeUntil
    float noticeUntil{0.0F};
};

} // namespace client
