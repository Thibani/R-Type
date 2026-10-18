#include "client/Replication.hpp"

#include "client/Components/Appearance.hpp"
#include "client/Components/Controls.hpp"
#include "client/Components/NetEntity.hpp"
#include "client/Components/Session.hpp"
#include "client/Components/SoundEvent.hpp"
#include "client/Singleton.hpp"
#include "game/Components/Motion.hpp"

#include <cstdint>
#include <iterator>
#include <string>
#include <variant>
#include <vector>

namespace client {

using namespace network::protocol;

namespace {

void playSound(ecs::World& world, Sound sound)
{
    world.addComponent(world.createEntity(), SoundEvent{.sound = sound});
}

void notify(ecs::World& world, std::string text, float now)
{
    Session& session = singleton<Session>(world);
    session.notice = std::move(text);
    session.noticeUntil = now + NoticeSeconds;
}

std::string playerLabel(std::uint8_t playerId)
{
    return "P" + std::to_string(playerId + 1);
}

bool isMissile(EntityKind kind) noexcept
{
    return kind == EntityKind::PlayerMissile || kind == EntityKind::EnemyMissile;
}

/// Creates or updates the entity of `netId` from network values.
void place(ecs::World& world, NetRegistry& registry, const NetEntity& data, std::int16_t x,
    std::int16_t y, std::int16_t vx, std::int16_t vy)
{
    ecs::Entity entity{};
    if (const auto found = registry.entities.find(data.netId); found != registry.entities.end()) {
        entity = found->second;
    } else {
        entity = world.createEntity();
        registry.entities.emplace(data.netId, entity);
        world.addComponent(entity, Appearance{.look = lookOf(data.kind), .player = data.owner});
    }
    world.addComponent(entity, data); // Replaces the previous values
    world.addComponent(entity, game::Position{.x = dequantize(x), .y = dequantize(y)});
    world.addComponent(entity, game::Velocity{.dx = dequantize(vx), .dy = dequantize(vy)});
}

void forgetOldDestroyed(NetRegistry& registry, float now)
{
    std::erase_if(registry.destroyedAt,
        [now](const auto& entry) { return now - entry.second >= DestroyedMemorySeconds; });
}

void apply(ecs::World& world, const EntitySpawn& spawn, float /*now*/)
{
    NetRegistry& registry = singleton<NetRegistry>(world);
    registry.destroyedAt.erase(spawn.netId); // The server decides: a spawn always counts
    place(world, registry,
        NetEntity{.netId = spawn.netId, .kind = spawn.kind, .owner = spawn.owner, .health = 0},
        spawn.x, spawn.y, spawn.vx, spawn.vy);
    if (isMissile(spawn.kind)) {
        playSound(world, Sound::Shot); // §4.3: a missile spawn is how firing is notified
    }
    if (spawn.kind == EntityKind::PlayerShip && spawn.owner < MaxPlayers) {
        singleton<Session>(world).players.at(spawn.owner).down = false; // Back in the game
    }
}

void apply(ecs::World& world, const EntityDestroy& destroy, float now)
{
    NetRegistry& registry = singleton<NetRegistry>(world);
    registry.destroyedAt[destroy.netId] = now;
    const auto found = registry.entities.find(destroy.netId);
    if (found == registry.entities.end()) {
        return;
    }
    world.destroyEntity(found->second);
    registry.entities.erase(found);
    if (destroy.reason == DestroyReason::Killed) {
        playSound(world, Sound::Explosion);
    }
}

void apply(ecs::World& world, const Snapshot& snapshot, float now)
{
    NetRegistry& registry = singleton<NetRegistry>(world);
    if (registry.newestTick && snapshot.serverTick < *registry.newestTick) {
        return; // §4.3: arrived out of order. Same tick = another part: applied
    }
    registry.newestTick = snapshot.serverTick;
    forgetOldDestroyed(registry, now);
    for (const EntityState& state : snapshot.entities) {
        if (!registry.entities.contains(state.netId)
            && registry.destroyedAt.contains(state.netId)) {
            continue; // §5.1: a late snapshot must not revive it
        }
        // §4.3: an unknown netId MAY be created from the snapshot (its spawn is late)
        place(world, registry,
            NetEntity{.netId = state.netId,
                .kind = state.kind,
                .owner = state.owner,
                .health = state.health},
            state.x, state.y, state.vx, state.vy);
    }
}

void apply(ecs::World& world, const PlayerJoined& joined, float now)
{
    if (joined.playerId >= MaxPlayers) {
        return;
    }
    Session& session = singleton<Session>(world);
    session.players.at(joined.playerId)
        = PlayerSlot{.present = true, .left = false, .down = false, .name = joined.name};
    if (joined.playerId != session.playerId && !session.catchingUp) {
        notify(world, playerLabel(joined.playerId) + " " + joined.name + " joined the game", now);
        playSound(world, Sound::PlayerJoined);
    }
}

void apply(ecs::World& world, const PlayerLeft& left, float now)
{
    if (left.playerId >= MaxPlayers) {
        return;
    }
    PlayerSlot& slot = singleton<Session>(world).players.at(left.playerId);
    slot.present = false;
    slot.left = true;
    const char* why = left.reason == LeaveReason::Timeout ? " (connection lost)"
        : left.reason == LeaveReason::Kicked              ? " (kicked)"
                                                          : "";
    notify(world, playerLabel(left.playerId) + " " + slot.name + " left the game" + why, now);
}

void apply(ecs::World& world, const PlayerDied& died, float now)
{
    if (died.playerId >= MaxPlayers) {
        return;
    }
    singleton<Session>(world).players.at(died.playerId).down = true;
    notify(world, playerLabel(died.playerId) + " is down", now);
    playSound(world, Sound::PlayerDown);
}

void apply(ecs::World& world, const GameState& state, float /*now*/)
{
    Session& session = singleton<Session>(world);
    session.score = state.score;
    Screen next = session.screen;
    switch (state.phase) {
    case GamePhase::WaitingForPlayers:
        next = Screen::WaitingRoom;
        break;
    case GamePhase::Running:
        next = Screen::Game;
        break;
    case GamePhase::GameOver:
        next = Screen::GameOver;
        break;
    }
    if (next != session.screen) {
        session.screen = next;
        session.focus = 0;
        session.menuOpen = false;
    }
}

template <typename Other>
void apply(ecs::World& /*world*/, const Other& /*payload*/, float /*now*/)
{
    // Session packets (ACCEPT, HEARTBEAT...) are handled by ServerConnection
}

} // namespace

void applyPayload(ecs::World& world, const Payload& payload, float now)
{
    // §8.1: the PLAYER_JOINED of the players already there come first. Any other
    // reliable event ends them; a SNAPSHOT does not (unreliable, it can overtake one)
    if (!std::holds_alternative<PlayerJoined>(payload)
        && !std::holds_alternative<Snapshot>(payload)) {
        singleton<Session>(world).catchingUp = false;
    }
    std::visit([&](const auto& packet) { apply(world, packet, now); }, payload);
}

void startSession(ecs::World& world, const ConnectAccept& accept, const std::string& name)
{
    clearSession(world);
    Session& session = singleton<Session>(world);
    session.playerId = accept.playerId;
    if (accept.playerId < MaxPlayers) {
        session.players.at(accept.playerId)
            = PlayerSlot{.present = true, .left = false, .down = false, .name = name};
    }
    // The current server spawns our ship at once and may never send GAME_STATE:
    // play now. A GAME_STATE (WAITING_FOR_PLAYERS) moves us to the waiting room.
    session.screen = Screen::Game;
    session.catchingUp = true;
    session.focus = 0;
    session.status.clear();
    session.error.clear();
}

void clearSession(ecs::World& world)
{
    std::vector<ecs::Entity> networked;
    world.query<NetEntity>(
        [&](ecs::Entity entity, NetEntity& /*data*/) { networked.push_back(entity); });
    for (const ecs::Entity entity : networked) {
        world.destroyEntity(entity);
    }
    singleton<NetRegistry>(world) = NetRegistry{};
    singleton<InputHistory>(world) = InputHistory{}; // A new session counts ticks from 0

    Session& session = singleton<Session>(world);
    session.playerId.reset();
    session.players = {};
    session.score = 0;
    session.menuOpen = false;
    session.notice.clear();
}

} // namespace client
