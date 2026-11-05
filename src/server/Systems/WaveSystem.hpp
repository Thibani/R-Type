#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Brings the enemies in, endlessly, while at least one ship is in the game.
/// Runs in the simulation, after `ProgressSystem`.
///
/// Every `waves::spawnInterval(level)` seconds comes an arrival: either one
/// enemy, or (with probability `waves::formationChance(level)`) a formation,
/// several enemies entering one after the other in a set pattern. What can
/// come depends on the level: each enemy and formation of the tables in the
/// .cpp has a level from which it appears, and heavy enemies grow more common
/// as the levels go by. Heights are random, chosen so that every member's
/// whole flight stays inside the arena.
///
/// No new arrival while a boss is in the game (`BossSystem`); the members of
/// a formation already on their way still come.
///
/// The next members of a formation wait in `WaveDirector::pending`. The
/// randomness comes from the seeded `WaveDirector` singleton, never from a
/// global: the same seed gives the same waves (handy in tests).
class WaveSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
