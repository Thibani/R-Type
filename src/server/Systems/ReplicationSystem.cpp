#include "server/Systems/ReplicationSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "game/Events/DespawnEvent.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/NetEntity.hpp"
#include "server/Components/Player.hpp"
#include "server/Singleton.hpp"

namespace server {

using namespace network::protocol;

ReplicationSystem::ReplicationSystem(ecs::World& world)
    : _world(world)
    , _onDespawn(world.events().subscribe<game::DespawnEvent>(
          [this](const game::DespawnEvent& despawn) { onDespawn(despawn); }))
{
}

void ReplicationSystem::onDespawn(const game::DespawnEvent& despawn)
{
    const auto* net = _world.tryGetComponent<NetEntity>(despawn.entity);
    if (net == nullptr) {
        return; // Not networked: clients never knew it
    }
    const auto* killer = _world.isAlive(despawn.killer)
        ? _world.tryGetComponent<NetEntity>(despawn.killer)
        : nullptr;
    const std::uint16_t killerNetId = killer != nullptr ? killer->netId : NoEntity;
    _destroyed[net->netId] = EntityDestroy{
        .netId = net->netId,
        .reason = static_cast<DestroyReason>(despawn.reason), // Same values (§4.3)
        .killerNetId = killerNetId,
    };
    if (const auto* ship = _world.tryGetComponent<PlayerShip>(despawn.entity)) {
        _deaths.push_back(PlayerDied{.playerId = ship->playerId, .killerNetId = killerNetId});
    }
}

void ReplicationSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    std::map<std::uint16_t, EntitySpawn> alive;
    world.query<NetEntity, game::Position, game::Velocity>([&](ecs::Entity /*entity*/,
                                                               NetEntity& net,
                                                               game::Position& position,
                                                               game::Velocity& velocity) {
        alive.emplace(net.netId,
            EntitySpawn{
                .netId = net.netId,
                .kind = net.kind,
                .owner = net.owner,
                .x = quantize(position.x),
                .y = quantize(position.y),
                .vx = quantize(velocity.dx),
                .vy = quantize(velocity.dy),
            });
    });

    const std::uint32_t score = singleton<TeamScore>(world).value;
    world.query<NetClient>([&](ecs::Entity /*entity*/, NetClient& client) {
        for (const PlayerDied& death : _deaths) {
            client.channel.queueReliable(death);
        }
        for (auto known = client.knownEntities.begin(); known != client.knownEntities.end();) {
            if (alive.contains(*known)) {
                ++known;
                continue;
            }
            const auto reason = _destroyed.find(*known);
            client.channel.queueReliable(
                reason != _destroyed.end() ? reason->second : EntityDestroy{.netId = *known});
            known = client.knownEntities.erase(known);
        }
        for (const auto& [netId, spawn] : alive) {
            if (client.knownEntities.insert(netId).second) {
                client.channel.queueReliable(spawn);
            }
        }
        if (client.knownScore != score) {
            client.channel.queueReliable(GameState{.phase = GamePhase::Running, .score = score});
            client.knownScore = score;
        }
    });
    _deaths.clear();
    _destroyed.clear();
}

} // namespace server
