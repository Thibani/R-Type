#pragma once

#include "ecs/System/System.hpp"

namespace ecs {
class World;
}

namespace client {

/// Creates the background stars: 3 layers scrolling slowly to the left, the
/// far ones slower (parallax). Speeds are in units per second: the scrolling
/// follows time, never the CPU speed.
void spawnStarfield(ecs::World& world);

/// Simulation: a star that left on the left comes back on the right.
class StarfieldSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace client
