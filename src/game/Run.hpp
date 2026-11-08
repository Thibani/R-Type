#pragma once

#include <cstdint>

/// A game from start to game over, shared by the server (which applies the
/// rules) and the client (which shows lives and score).
///
/// The team shares `StartingLives` lives. Each ship destroyed costs one; a
/// ship destroyed when none is left does not come back. When no ship is left
/// in the game, it is GAME OVER, and a player can start a new game.
///
/// The team score counts kills (each enemy's points) and distance: while a
/// ship is in the game, `DistancePointsPerSecond × level` points per second,
/// so surviving deeper is worth more.
namespace game::run {

/// Ships the team can lose in one game.
inline constexpr std::uint8_t StartingLives = 3;

/// Lives can be won back (a boss kill gives one), up to this.
inline constexpr std::uint8_t MaxLives = 5;

/// Points per second of play at level 1, multiplied by the level.
inline constexpr std::uint32_t DistancePointsPerSecond = 10;

} // namespace game::run
