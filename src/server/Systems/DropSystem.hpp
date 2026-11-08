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

/// When an enemy is killed (`DespawnEvent`, KILLED), it may leave a pickup
/// where it was (`game/Gadgets.hpp`): a repair kit (`RepairChance`) or a
/// Force capsule (`ForceChance`), never both. A boss always leaves both.
/// Enemies leaving the arena drop nothing.
///
/// The dice come from the `DropTable` singleton, so a game can be replayed.
///
///     world.systems().add<DropSystem>(world);
class DropSystem final : public ecs::System {
public:
    explicit DropSystem(ecs::World& world);

    /// Nothing to do each step: everything happens on a kill.
    void update(ecs::World& world, const ecs::Time& time) override;

private:
    void onDespawn(const game::DespawnEvent& despawn);

    ecs::World& _world;
    ecs::Subscription _onDespawn;
};

} // namespace server
