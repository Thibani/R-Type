#include "server/Systems/PickupSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Components/Health.hpp"
#include "game/Components/Motion.hpp"
#include "game/Events/CollisionEvent.hpp"
#include "game/Events/DespawnEvent.hpp"
#include "game/Gadgets.hpp"
#include "game/Ship.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/Gadgets.hpp"
#include "server/Components/Player.hpp"
#include "server/Singleton.hpp"
#include "server/Spawn.hpp"

#include <cstdint>

namespace server {

PickupSystem::PickupSystem(ecs::World& world)
    : _world(world)
    , _onCollision(world.events().subscribe<game::CollisionEvent>(
          [this](const game::CollisionEvent& collision) { onCollision(collision); }))
{
}

void PickupSystem::update(ecs::World& /*world*/, const ecs::Time& /*time*/) { }

void PickupSystem::onCollision(const game::CollisionEvent& collision)
{
    take(collision.first, collision.second);
    take(collision.second, collision.first);
}

void PickupSystem::take(ecs::Entity ship, ecs::Entity pickup)
{
    if (!_world.isAlive(ship) || !_world.isAlive(pickup)) {
        return;
    }
    auto* item = _world.tryGetComponent<Pickup>(pickup);
    const auto* owner = _world.tryGetComponent<PlayerShip>(ship);
    auto* health = _world.tryGetComponent<game::Health>(ship);
    if (item == nullptr || owner == nullptr || item->taken || health == nullptr
        || health->current == 0) {
        return; // Not a ship and a pickup, already taken, or a ship being destroyed
    }
    item->taken = true;
    const std::uint8_t playerId = owner->playerId;
    if (item->kind == PickupKind::Repair) {
        health->current = game::ship::MaxHealth;
    } else if (hasForce(playerId)) {
        singleton<TeamScore>(_world).value += game::pickup::ForceBonusPoints;
    } else {
        const game::Position at = _world.getComponent<game::Position>(ship);
        // Events are delivered outside of any query: entities can be created here
        (void)spawnForce(_world, playerId, at);
    }
    _world.events().emit(game::DespawnRequest{
        .entity = pickup, .reason = game::DespawnReason::Expired, .killer = ship});
}

bool PickupSystem::hasForce(std::uint8_t playerId)
{
    bool found = false;
    _world.query<Force>(
        [&](ecs::Entity /*entity*/, Force& force) { found = found || force.playerId == playerId; });
    return found;
}

} // namespace server
