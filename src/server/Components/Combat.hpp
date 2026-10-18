#pragma once

#include "game/Bydo.hpp"

#include <cstdint>
#include <random>

namespace server {

/// Seconds before the ship may fire again, on a player's ship.
struct FireCooldown {
    float remaining{};
};

/// Marks a Bydo (enemy) and drives its wave and its shots.
struct Bydo {
    float baseY{}; // Centre line of its wave
    float age{}; // Seconds since it spawned
    float fireCooldown{}; // Seconds before its next shot
};

/// When the next Bydo appears, and the random source for where, on a
/// singleton entity. Seeded once: the same seed gives the same waves.
struct BydoSpawner {
    float untilNext{game::bydo::SpawnInterval}; // The players get a moment first
    std::minstd_rand random{0x52545950}; // "RTYP"
};

/// The shared team score, on a singleton entity. Sent in GAME_STATE.
struct TeamScore {
    std::uint32_t value{};
};

} // namespace server
