#pragma once

#include "network/Protocol/Packets.hpp"

namespace ecs {
class World;
} // namespace ecs

/// The game as a whole, from start to game over (see `game/Run.hpp`).
namespace server {

/// What GAME_STATE tells the clients now: phase, team score, level, lives and
/// distance to the next boss.
[[nodiscard]]
network::protocol::GameState currentState(ecs::World& world);

/// Starts a new game: score, level, waves and lives back to their start, every
/// enemy and projectile despawned (EXPIRED), every player given a ship again
/// by `PlayerShipSystem`. Call it outside of any query.
void startNewGame(ecs::World& world);

} // namespace server
