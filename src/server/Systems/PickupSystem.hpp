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

/// A ship touching a `Pickup` takes it (`game/Gadgets.hpp`):
///
///   - a repair kit gives the ship back all its hit points;
///   - a Force capsule gives its player a Force (`spawnForce`), or
///     `ForceBonusPoints` to the team if they already have one.
///
/// The pickup is then removed (EXPIRED: it was used, not destroyed).
///
///     world.systems().add<PickupSystem>(world); // after game::CollisionSystem
class PickupSystem final : public ecs::System {
public:
    explicit PickupSystem(ecs::World& world);

    /// Nothing to do each step: everything happens on a collision.
    void update(ecs::World& world, const ecs::Time& time) override;

private:
    void onCollision(const game::CollisionEvent& collision);
    /// `ship` takes `pickup`, if they are a ship and a pickup.
    void take(ecs::Entity ship, ecs::Entity pickup);
    /// True if player `playerId` already has a Force.
    bool hasForce(std::uint8_t playerId);

    ecs::World& _world;
    ecs::Subscription _onCollision;
};

} // namespace server
