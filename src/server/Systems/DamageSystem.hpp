#pragma once

#include "ecs/Entity/Entity.hpp"
#include "ecs/Event/Subscription.hpp"
#include "ecs/System/System.hpp"

#include <unordered_set>

namespace ecs {
class World;
} // namespace ecs

namespace game {
struct CollisionEvent;
} // namespace game

namespace server {

/// Decides what a collision means (#58, #59). `game::CollisionEvent` only
/// says that two hitboxes overlap; this system applies the rules:
///
/// | Collision                 | Result                                          |
/// |---------------------------|-------------------------------------------------|
/// | player missile × Bydo     | missile gone, Bydo −1 HP                        |
/// | Bydo missile × ship       | missile gone, ship −1 HP                        |
/// | Bydo × ship               | Bydo killed, ship −1 HP                         |
///
/// An entity at 0 HP is killed: a `DespawnRequest` (KILLED, with its killer),
/// so `ReplicationSystem` sends ENTITY_DESTROY with the real reason, and
/// PLAYER_DIED for a ship. A killed Bydo adds `bydo::Points` to the
/// `TeamScore`; a killed ship's player gets a `RespawnTimer`.
///
/// A missile hits once: one that already hit something this step, or an
/// entity already killed, ignores the other overlaps of the same step.
///
///     world.systems().add<DamageSystem>(world); // after game::CollisionSystem
class DamageSystem final : public ecs::System {
public:
    explicit DamageSystem(ecs::World& world);

    /// Forgets the entities used up during the previous step.
    void update(ecs::World& world, const ecs::Time& time) override;

private:
    void onCollision(const game::CollisionEvent& collision);
    /// Removes one hit point from `target`, killing it at 0.
    void hit(ecs::Entity target, ecs::Entity by);
    void kill(ecs::Entity target, ecs::Entity by);

    ecs::World& _world;
    std::unordered_set<ecs::Entity> _spent; // Hit or killed this step
    ecs::Subscription _onCollision;
};

} // namespace server
