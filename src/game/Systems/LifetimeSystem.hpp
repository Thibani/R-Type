#pragma once

#include "ecs/System/System.hpp"

namespace game {

/// Counts down every `Lifetime` and asks for the despawn of the entities
/// whose time is up (`DespawnRequest` with the reason `Expired`).
///
/// It only asks: `DespawnSystem` announces and destroys the entity.
class LifetimeSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace game
