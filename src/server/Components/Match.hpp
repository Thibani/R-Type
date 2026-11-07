#pragma once

#include "game/Run.hpp"
#include "network/Protocol/Protocol.hpp"

#include <cstdint>

namespace server {

/// The current game, on a singleton entity: is it running or over, and how
/// many lives the team has left (see `game/Run.hpp`). Sent in GAME_STATE.
struct Match {
    network::protocol::GamePhase phase{network::protocol::GamePhase::Running};
    std::uint8_t lives{game::run::StartingLives};

    friend bool operator==(const Match&, const Match&) = default;
};

} // namespace server
