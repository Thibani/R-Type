#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// The Bydos' "brain" for shooting (#57): each Bydo pulls its trigger and
/// aims at the nearest ship. No ship in the game: no shot. `WeaponSystem`
/// fires, at the rate of the Bydo's weapon. Runs before `WeaponSystem`.
class BydoAimSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
