#include "server/Systems/DamageSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Bydo.hpp"
#include "game/Components/Collision.hpp"
#include "game/Components/Health.hpp"
#include "game/Events/CollisionEvent.hpp"
#include "game/Events/DespawnEvent.hpp"
#include "game/Ship.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/Player.hpp"
#include "server/Singleton.hpp"

#include <utility>

namespace server {

namespace {

game::LayerMask layerOf(ecs::World& world, ecs::Entity entity)
{
    const auto* hitbox = world.tryGetComponent<game::Hitbox>(entity);
    return hitbox != nullptr ? hitbox->layer : game::layer::None;
}

} // namespace

DamageSystem::DamageSystem(ecs::World& world)
    : _world(world)
    , _onCollision(world.events().subscribe<game::CollisionEvent>(
          [this](const game::CollisionEvent& collision) { onCollision(collision); }))
{
}

void DamageSystem::update(ecs::World& /*world*/, const ecs::Time& /*time*/)
{
    _spent.clear();
}

void DamageSystem::onCollision(const game::CollisionEvent& collision)
{
    ecs::Entity a = collision.first;
    ecs::Entity b = collision.second;
    if (_spent.contains(a) || _spent.contains(b) || !_world.isAlive(a) || !_world.isAlive(b)) {
        return;
    }
    // Sorted by layer bit, so each rule below has a single order to check
    if (layerOf(_world, a) > layerOf(_world, b)) {
        std::swap(a, b);
    }
    const game::LayerMask first = layerOf(_world, a);
    const game::LayerMask second = layerOf(_world, b);

    if (first == game::layer::PlayerMissile && second == game::layer::Enemy) {
        kill(a, b); // The missile is used up
        hit(b, a);
    } else if (first == game::layer::PlayerShip && second == game::layer::EnemyMissile) {
        kill(b, a);
        hit(a, b);
    } else if (first == game::layer::PlayerShip && second == game::layer::Enemy) {
        kill(b, a); // Ramming kills the Bydo, and costs the ship a heart
        hit(a, b);
    }
}

void DamageSystem::hit(ecs::Entity target, ecs::Entity by)
{
    auto* health = _world.tryGetComponent<game::Health>(target);
    if (health == nullptr || health->current == 0) {
        return;
    }
    if (--health->current == 0) {
        kill(target, by);
    }
}

void DamageSystem::kill(ecs::Entity target, ecs::Entity by)
{
    if (!_spent.insert(target).second) {
        return;
    }
    if (auto* health = _world.tryGetComponent<game::Health>(target)) {
        health->current = 0;
    }
    if (_world.hasComponent<Bydo>(target)) {
        singleton<TeamScore>(_world).value += game::bydo::Points;
    }
    if (const auto* ship = _world.tryGetComponent<PlayerShip>(target)) {
        const std::uint8_t playerId = ship->playerId;
        _world.query<NetClient>([&](ecs::Entity player, NetClient& client) {
            if (client.playerId == playerId) {
                _world.commands().addComponent(
                    player, RespawnTimer{.remaining = game::ship::RespawnDelay});
            }
        });
    }
    _world.events().emit(
        game::DespawnRequest{.entity = target, .reason = game::DespawnReason::Killed, .killer = by});
}

} // namespace server
