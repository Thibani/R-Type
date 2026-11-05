#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Counts the seconds played and turns them into the level of the run
/// (`RunProgress`, `waves::levelAt`). The clock only runs while at least one
/// ship is in the game: a player waiting to respawn does not lose levels of
/// preparation. Runs in the simulation, before `WaveSystem`.
///
/// While it runs, it also adds the distance points to the `TeamScore`:
/// `run::DistancePointsPerSecond × level` per second (see `game/Run.hpp`).
class ProgressSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
