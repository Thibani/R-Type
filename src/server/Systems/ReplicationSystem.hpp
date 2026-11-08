#pragma once

#include "ecs/Event/Subscription.hpp"
#include "ecs/System/System.hpp"
#include "network/Protocol/Packets.hpp"
#include "server/Network.hpp"

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace ecs {
class World;
} // namespace ecs

namespace game {
struct DespawnEvent;
} // namespace game

namespace server {

/// Tells every client which networked entities exist (docs/protocol.md
/// §4.3): ENTITY_SPAWN for each `NetEntity` the client was not told about,
/// ENTITY_DESTROY for each one it knows that is gone. Runs in `frameEnd()`,
/// after `SessionCleanupSystem` and before `NetSendSystem`.
///
/// Each `NetClient` remembers the netIds it was sent (`knownEntities`), so a
/// newcomer gets one spawn per existing entity (§8.1) with the same code that
/// announces new entities to everyone. The events follow the PLAYER_JOINED
/// queued in `frameStart()`, as §8.1 orders.
///
/// Destroys carry the real reason and killer: it listens to
/// `game::DespawnEvent` (the entity is still alive then, so its netId and its
/// killer's can be read). The death of a ship is also sent as PLAYER_DIED.
/// An entity removed without despawn (the ship of a player who left) is
/// destroyed as KILLED by no one.
///
/// It also sends GAME_STATE (`currentState`) to each client that has not seen
/// the current one: at once to every newcomer and on every change of phase,
/// level, lives or boss distance; at most every `ScoreUpdateInterval` when only
/// the score moved (distance points change it nearly every step).
///
/// It uses the time point of the frame, so the tests control it.
///
/// shortcut: no lobby (WAITING_FOR_PLAYERS): a game starts as soon as a
/// player connects.
class ReplicationSystem final : public ecs::System {
public:
    static constexpr std::chrono::milliseconds ScoreUpdateInterval{250};

    /// `world` is the game's, `sessions` the one holding the `NetClient`s.
    ReplicationSystem(ecs::World& world, ecs::World& sessions);
    /// The players' `NetClient`s are in `world` itself: for the tests of a single World.
    explicit ReplicationSystem(ecs::World& world);

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    void onDespawn(const game::DespawnEvent& despawn);

    ecs::World& _world;
    ecs::World& _sessions;
    std::optional<TimePoint> _lastScoreUpdate; // Last GAME_STATE sent for the score alone
    // Gathered during the simulation steps, sent at the end of the frame
    std::map<std::uint16_t, network::protocol::EntityDestroy> _destroyed;
    std::vector<network::protocol::PlayerDied> _deaths;
    ecs::Subscription _onDespawn;
};

} // namespace server
