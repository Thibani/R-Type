#pragma once

#include "ecs/System/System.hpp"

namespace client {

/// Simulation: `Position += Velocity × step` for every moving entity.
///
/// Between two snapshots (30 Hz), entities keep moving with the velocity the
/// server sent, so the 60 Hz display stays smooth; the next snapshot corrects
/// any difference. Stars use it too.
class MotionSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace client
