#pragma once

#include "ecs/Entity/Entity.hpp"
#include "ecs/Event/Subscription.hpp"
#include "ecs/System/System.hpp"

namespace ecs {
class World;
} // namespace ecs

namespace game {
struct CollisionEvent;
} // namespace game

namespace server {

/// Drives every `Force` (rules in `game/Gadgets.hpp`), after the ships have
/// moved (`game::ConfinementSystem`) and before the collisions:
///
///   - a press of its player's POD key moves it to the next slot around the
///     ship (front, above, behind, below);
///   - it sticks to its ship in that slot, and takes the ship's velocity so
///     the clients interpolate it smoothly;
///   - its trigger is pulled while the ship's is, pointing away from the
///     ship (`WeaponSystem` fires it);
///   - a Force whose ship is gone (destroyed, or its player left) goes too.
///
/// An entity with `BlocksMissiles` (the Force) stops the enemy missiles it
/// touches: they are removed as EXPIRED (absorbed, no explosion).
///
///     world.systems().add<ForceSystem>(world); // after game::ConfinementSystem
class ForceSystem final : public ecs::System {
public:
    explicit ForceSystem(ecs::World& world);

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    void onCollision(const game::CollisionEvent& collision);
    /// Removes `missile` if it is an enemy missile and `blocker` blocks it.
    void block(ecs::Entity blocker, ecs::Entity missile);

    ecs::World& _world;
    ecs::Subscription _onCollision;
};

} // namespace server
