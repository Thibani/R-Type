#pragma once

#include "ecs/World/World.hpp"
#include "network/Protocol/Packets.hpp"

#include <string>

/// Turns what the server says into the client's world: entities for
/// ENTITY_SPAWN / ENTITY_DESTROY / SNAPSHOT, the `Session` for the player and
/// game events. Plain functions with no window and no socket, so the tests
/// can feed them packets directly.
namespace client {

/// Seconds an ENTITY_DESTROY keeps its netId out of snapshots (§5.1).
inline constexpr float DestroyedMemorySeconds = 5.0F;
/// Seconds a notice ("P4 left the game") stays on screen.
inline constexpr float NoticeSeconds = 4.0F;

/// Applies one game payload. `now` is the client's elapsed time in seconds.
void applyPayload(ecs::World& world, const network::protocol::Payload& payload, float now);

/// CONNECT_ACCEPT received: we are player `accept.playerId`, called `name`.
void startSession(
    ecs::World& world, const network::protocol::ConnectAccept& accept, const std::string& name);

/// Forgets the game: every network entity, the players, the score.
void clearSession(ecs::World& world);

} // namespace client
