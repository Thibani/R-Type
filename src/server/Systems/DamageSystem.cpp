#include "server/Systems/DamageSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Components/Collision.hpp"
#include "game/Components/Health.hpp"
#include "game/Events/CollisionEvent.hpp"
#include "game/Events/DespawnEvent.hpp"
#include "game/Ship.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/Player.hpp"
#include "server/Singleton.hpp"

namespace server {

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
    // Either side may hurt the other (a missile and its target)
    attack(collision.first, collision.second);
    attack(collision.second, collision.first);
}

void DamageSystem::attack(ecs::Entity attacker, ecs::Entity target)
{
    if (_spent.contains(attacker) || _spent.contains(target) || !_world.isAlive(attacker)
        || !_world.isAlive(target)) {
        return;
    }
    const auto* damage = _world.tryGetComponent<Damage>(attacker);
    const auto* attackerBox = _world.tryGetComponent<game::Hitbox>(attacker);
    const auto* targetBox = _world.tryGetComponent<game::Hitbox>(target);
    if (damage == nullptr || attackerBox == nullptr || targetBox == nullptr
        || (attackerBox->collidesWith & targetBox->layer) == 0) {
        return;
    }
    const std::uint8_t amount = damage->amount;
    if (damage->diesOnHit) {
        kill(attacker, target); // Used up, or crashed into it
    }
    hit(target, attacker, amount);
}

void DamageSystem::hit(ecs::Entity target, ecs::Entity by, std::uint8_t amount)
{
    auto* health = _world.tryGetComponent<game::Health>(target);
    if (health == nullptr || health->current == 0) {
        return;
    }
    health->current = amount >= health->current ? 0 : static_cast<std::uint8_t>(health->current - amount);
    if (health->current == 0) {
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
    if (const auto* worth = _world.tryGetComponent<ScoreValue>(target)) {
        singleton<TeamScore>(_world).value += worth->points;
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
