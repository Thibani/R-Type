#pragma once

#include "ecs/World/World.hpp"
#include "network/Protocol/Packets.hpp"

#include <cstdint>

/// Turns what the server says into the client's world: entities for
/// ENTITY_SPAWN / ENTITY_DESTROY / SNAPSHOT, the `Session` for the player and
/// game events. Plain functions with no window and no socket, so the tests
/// can feed them packets directly.
namespace client {

/// Seconds an ENTITY_DESTROY keeps its netId out of snapshots (§5.1).
inline constexpr float DestroyedMemorySeconds = 5.0F;
/// Seconds a notice ("P4 Dave left the game") stays on screen.
inline constexpr float NoticeSeconds = 4.0F;

/// Applies one game payload. `now` is the client's elapsed time in seconds.
/// Outside the game, only JOIN_RESULT counts: what still arrives from a game we
/// left must not bring it back on the main page.
void applyPayload(ecs::World& world, const network::protocol::Payload& payload, float now);

/// Logged in: to the main page.
void startSession(ecs::World& world);

/// JOIN_RESULT accepted: we are player `playerId`, called `Session::name`.
void enterGame(ecs::World& world, std::uint8_t playerId);

/// Leaves the game for the main page. The session goes on: it keeps counting input
/// ticks and remembers the newest snapshot, so nothing late from this game counts
/// in the next one.
void leaveGame(ecs::World& world);

/// Forgets the session: the game, and everything `leaveGame` keeps.
void clearSession(ecs::World& world);

} // namespace client
