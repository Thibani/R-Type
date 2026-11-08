#pragma once

#include "ecs/Entity/Entity.hpp"
#include "game/Boss.hpp"
#include "game/Waves.hpp"

#include <cstdint>
#include <random>
#include <vector>

namespace ecs {
class World;
} // namespace ecs

/// The endless mode's state, on singleton entities (see `game/Waves.hpp` for
/// the rules and `WaveSystem` for how enemies are chosen).
namespace server {

/// How far the run has gone: seconds played (only while a ship is in the
/// game) and the level they give. Written by `ProgressSystem`.
struct RunProgress {
    float elapsed{};
    std::uint32_t level{1};
    float distancePoints{}; // Distance points earned, not yet added to the score
    float nextBossAt{game::boss::Interval}; // `elapsed` when the next boss comes
    std::uint32_t bossesDefeated{};
};

/// Creates one enemy at height `y` (see `Spawn.hpp`).
using SpawnFunction = ecs::Entity (*)(ecs::World& world, float y, float firstShotDelay);

/// An enemy that will enter in `delay` seconds: the next members of a
/// formation wait here.
struct PendingSpawn {
    float delay{};
    SpawnFunction spawn{};
    float y{};
    float firstShotDelay{};
};

/// When the next arrival is due, the enemies about to enter, and the random
/// source for what comes and where. Seeded once: the same seed gives the same
/// waves (tests, replays).
struct WaveDirector {
    float untilNext{game::waves::BaseSpawnInterval}; // The players get a moment first
    // Fixed on purpose: the same seed gives the same waves (tests, replays)
    std::minstd_rand random{0x52545950}; // NOLINT(bugprone-random-generator-seed) "RTYP"
    std::vector<PendingSpawn> pending;
};

} // namespace server
