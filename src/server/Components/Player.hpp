#pragma once

#include <cstdint>

namespace server {

/// The latest input of a player, on the player's entity (the one holding its
/// `NetClient`), written by `InputSystem` and read by gameplay systems.
///
/// It lives on the player, not on the ship: the player keeps sending inputs
/// while their ship is dead or not spawned yet.
struct PlayerInput {
    std::uint8_t mask{}; // network::protocol::input bits: Up, Down, Left, Right, Fire
    std::uint32_t lastTick{}; // Client input tick of `mask`, echoed in snapshots
    bool received{false}; // False until the first INPUT: lastTick is meaningless
};

/// On a player whose ship was destroyed: seconds before a new one appears.
struct RespawnTimer {
    float remaining{};
};

/// Marks a player's ship, a separate entity from the player: a ship can be
/// destroyed (killed) while its player stays connected.
struct PlayerShip {
    std::uint8_t playerId{};
};

} // namespace server
