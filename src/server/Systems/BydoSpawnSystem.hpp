#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Brings a new Bydo in from the right edge every `bydo::SpawnInterval`
/// seconds (#55), at a random height, while at least one ship is in the game.
/// Runs in the simulation.
///
/// The randomness comes from the seeded `BydoSpawner` singleton, never from
/// a global: the same seed gives the same waves (handy in tests).
class BydoSpawnSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
