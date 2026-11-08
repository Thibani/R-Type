#include "server/Systems/LivesSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Events/DespawnEvent.hpp"
#include "game/Ship.hpp"
#include "server/Components/Match.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/Player.hpp"
#include "server/Singleton.hpp"

#include <cstdint>

namespace server {

LivesSystem::LivesSystem(ecs::World& world)
    : _world(world)
    , _onDespawn(world.events().subscribe<game::DespawnEvent>(
          [this](const game::DespawnEvent& despawn) { onDespawn(despawn); }))
{
}

void LivesSystem::update(ecs::World& /*world*/, const ecs::Time& /*time*/) { }

void LivesSystem::onDespawn(const game::DespawnEvent& despawn)
{
    const auto* ship = _world.tryGetComponent<PlayerShip>(despawn.entity);
    if (ship == nullptr || despawn.reason != game::DespawnReason::Killed) {
        return;
    }
    auto& match = singleton<Match>(_world);
    if (match.lives > 0) {
        --match.lives;
    }
    const std::uint8_t playerId = ship->playerId;
    const bool comesBack = match.lives > 0;
    _world.query<NetClient>([&](ecs::Entity player, const NetClient& client) {
        if (client.playerId != playerId) {
            return;
        }
        // Commands: the World is being iterated, and events are being delivered
        if (comesBack) {
            _world.commands().addComponent(
                player, RespawnTimer{.remaining = game::ship::RespawnDelay});
        } else {
            _world.commands().addComponent(player, Eliminated{});
        }
    });
}

} // namespace server
