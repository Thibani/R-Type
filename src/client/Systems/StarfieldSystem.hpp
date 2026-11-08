#pragma once

#include "ecs/System/System.hpp"

namespace ecs {
class World;
}

namespace client {

/// Creates the background stars: 3 layers scrolling slowly to the left, the
/// far ones slower (parallax). Speeds are in units per second: the scrolling
/// follows time, never the CPU speed. Also creates the gold streaks.
void spawnStarfield(ecs::World& world);

/// Simulation: a star that left on the left comes back on the right, and a
/// streak that left at the top right comes back from the bottom left.
class StarfieldSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace client
