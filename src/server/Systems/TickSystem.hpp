#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Counts simulation steps in the `SimulationTick` singleton. Runs first in
/// the simulation (`world.systems()`), so every other system sees the tick of
/// the step being computed.
class TickSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
