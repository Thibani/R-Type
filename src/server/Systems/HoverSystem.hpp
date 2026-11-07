#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Drives every `Hover` through its three phases:
///
///     coming in   x > stopX        flies left at speed
///     holding     at stopX         stops, moves up and down (bob)
///     leaving     after holdSeconds flies left at speed (never if 0)
///
/// Gunners hold a few seconds; the boss holds until it dies. It only sets
/// the velocity; runs before `game::MovementSystem`.
class HoverSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
