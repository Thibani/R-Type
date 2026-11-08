#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Steers every `Seeker` towards the nearest ship, vertically only: it keeps
/// flying left at its speed, and its vertical speed changes by at most
/// `verticalAcceleration` per second, up to `maxVerticalSpeed`, so it curves
/// smoothly and can be dodged. Without ships, it flies straight.
///
/// It only sets the velocity; `MovementSystem` moves it. Runs before
/// `game::MovementSystem`.
class SeekerSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
