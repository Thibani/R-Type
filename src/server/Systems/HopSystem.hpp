#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Makes every `Hop` jump along the floor: gravity pulls it down, and each
/// time it lands (at `floorY`, falling) it jumps again at `jumpSpeed`. Its
/// horizontal speed is left as it is. Runs before `game::MovementSystem`.
class HopSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
