#pragma once

#include "game/Gadgets.hpp"

#include <cstdint>
#include <random>

/// The gadgets as data (rules in `game/Gadgets.hpp`): pickups dropped by
/// enemies, and the Force attached to a ship.
namespace server {

/// What a pickup gives to the ship that touches it (`PickupSystem`).
enum class PickupKind : std::uint8_t {
    Repair, // Full health
    Force, // A Force, or points if the ship already has one
};

/// A pickup, drifting left until a ship takes it or it leaves the arena.
struct Pickup {
    PickupKind kind{PickupKind::Repair};
    bool taken{false}; // Two ships touching it in the same step: the first one has it
};

/// A Force (`ForceSystem`), owned by player `playerId`: it follows that
/// player's ship in `slot`, fires with it and is lost with it.
struct Force {
    std::uint8_t playerId{};
    game::force::Slot slot{game::force::Slot::Front};
    bool podHeld{true}; // POD at the last step: a press is a change to held
};

/// Stops the enemy missiles it touches (the Force).
struct BlocksMissiles { };

/// The random source of the drops, on a singleton entity: the same seed
/// gives the same drops, so games and tests are reproducible.
struct DropTable {
    std::minstd_rand random{0x50494B55}; // NOLINT(bugprone-random-generator-seed) "PIKU"
};

} // namespace server
