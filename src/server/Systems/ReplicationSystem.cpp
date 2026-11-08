#include "server/Systems/ReplicationSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "game/Events/DespawnEvent.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/NetEntity.hpp"
#include "server/Components/Player.hpp"
#include "server/Run.hpp"
#include "server/Singleton.hpp"

namespace server {

using namespace network::protocol;

namespace {

enum class StateUpdate : std::uint8_t {
    None, // The client knows this state
    ScoreOnly, // Only the score moved: can wait a little
    Now, // New client, or phase, level, lives or boss distance changed
};

StateUpdate stateUpdate(const std::optional<GameState>& known, const GameState& state)
{
    if (!known) {
        return StateUpdate::Now;
    }
    if (*known == state) {
        return StateUpdate::None;
    }
    GameState sameScore = state;
    sameScore.score = known->score;
    return *known == sameScore ? StateUpdate::ScoreOnly : StateUpdate::Now;
}

} // namespace

ReplicationSystem::ReplicationSystem(ecs::World& world)
    : ReplicationSystem(world, world)
{
}

ReplicationSystem::ReplicationSystem(ecs::World& world, ecs::World& sessions)
    : _world(world)
    , _sessions(sessions)
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

void ReplicationSystem::update(ecs::World& world, const ecs::Time& time)
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

    const GameState state = currentState(world);
    const TimePoint now = toTimePoint(time);
    const bool scoreDue = !_lastScoreUpdate || now - *_lastScoreUpdate >= ScoreUpdateInterval;
    bool scoreSent = false;
    world.query<InGame>([&](ecs::Entity /*entity*/, InGame& player) {
        NetClient& client = _sessions.getComponent<NetClient>(player.session);
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
        const StateUpdate update = stateUpdate(client.knownState, state);
        if (update == StateUpdate::Now || (update == StateUpdate::ScoreOnly && scoreDue)) {
            client.channel.queueReliable(state);
            client.knownState = state;
            scoreSent = scoreSent || update == StateUpdate::ScoreOnly;
        }
    });
    if (scoreSent) {
        _lastScoreUpdate = now;
    }
    _deaths.clear();
    _destroyed.clear();
}

} // namespace server
