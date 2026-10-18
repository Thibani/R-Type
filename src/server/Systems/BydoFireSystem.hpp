#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Makes every Bydo fire at the nearest ship every `bydo::FireInterval`
/// seconds (#57). The missile flies straight at where the ship was. No ship
/// in the game: no shot. Runs in the simulation, before movement.
class BydoFireSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
