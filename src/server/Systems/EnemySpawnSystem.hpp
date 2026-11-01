#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Brings a new enemy in from the right edge every `enemies::SpawnInterval`
/// seconds, while at least one ship is in the game. Runs in the simulation.
///
/// Which enemy is drawn at random, weighted so that small enemies are common
/// and heavy ones rare (`SpawnTable` in the .cpp); its height is random too,
/// chosen so that its whole flight stays inside the arena.
///
/// The randomness comes from the seeded `EnemySpawner` singleton, never from
/// a global: the same seed gives the same waves (handy in tests).
class EnemySpawnSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
