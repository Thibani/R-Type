#pragma once

#include "ecs/Event/Subscription.hpp"
#include "ecs/System/System.hpp"

namespace ecs {
class World;
} // namespace ecs

namespace game {
struct DespawnEvent;
} // namespace game

namespace server {

/// When an entity with `BurstOnDeath` is killed (`DespawnEvent`, KILLED), it
/// fires its projectiles all around, evenly spread: a Mine shot down still
/// threatens what is close to it. Leaving the arena does not burst.
///
///     world.systems().add<DeathBurstSystem>(world);
class DeathBurstSystem final : public ecs::System {
public:
    explicit DeathBurstSystem(ecs::World& world);

    /// Nothing to do each step: everything happens on a kill.
    void update(ecs::World& world, const ecs::Time& time) override;

private:
    void onDespawn(const game::DespawnEvent& despawn);

    ecs::World& _world;
    ecs::Subscription _onDespawn;
};

} // namespace server
