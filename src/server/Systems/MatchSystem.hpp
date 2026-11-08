#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Decides when a game ends (see `game/Run.hpp`). Runs in the simulation,
/// first after `TickSystem`.
///
///  - GAME OVER: the game is running, the team has no life left, and no ship
///    is in the game or about to come back (`RespawnTimer`). The waves stop
///    by themselves (no ship), the clients get GAME_STATE (GAME_OVER) and a
///    player can start a new game (`RestartSystem`).
///  - New game: once every player has left, the next one to connect starts
///    afresh instead of joining a finished or half-played game.
class MatchSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
