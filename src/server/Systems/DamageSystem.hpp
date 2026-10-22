#pragma once

#include "ecs/Entity/Entity.hpp"
#include "ecs/Event/Subscription.hpp"
#include "ecs/System/System.hpp"

#include <cstdint>
#include <unordered_set>

namespace ecs {
class World;
} // namespace ecs

namespace game {
struct CollisionEvent;
} // namespace game

namespace server {

/// Decides what a collision means (#58, #59). `game::CollisionEvent` only
/// says that two hitboxes overlap; the rules come from the entities' data:
///
///   an entity with `Damage` hurts what its `Hitbox::collidesWith` names:
///   the other loses `Damage::amount` HP, and the attacker dies if
///   `Damage::diesOnHit` (a missile is used up, a Bydo crashes).
///
/// Today: player missiles hurt Bydos, Bydo missiles and Bydos hurt ships.
/// A new weapon or enemy needs new data (`Spawn.hpp`), not a new rule here.
///
/// An entity at 0 HP is killed: a `DespawnRequest` (KILLED, with its killer),
/// so `ReplicationSystem` sends ENTITY_DESTROY with the real reason, and
/// PLAYER_DIED for a ship. A killed entity with a `ScoreValue` adds its points
/// to the `TeamScore`; a killed ship's player gets a `RespawnTimer`.
///
/// An attacker hits once: one that already hit something this step, or an
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
    /// `attacker` hurts `target` if its `Damage` and `Hitbox` say so.
    void attack(ecs::Entity attacker, ecs::Entity target);
    /// Removes `amount` hit points from `target`, killing it at 0.
    void hit(ecs::Entity target, ecs::Entity by, std::uint8_t amount);
    void kill(ecs::Entity target, ecs::Entity by);

    ecs::World& _world;
    std::unordered_set<ecs::Entity> _spent; // Hit or killed this step
    ecs::Subscription _onCollision;
};

} // namespace server
