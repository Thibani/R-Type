#pragma once

#include "ecs/Loop/GameLoop.hpp"
#include "ecs/World/World.hpp"
#include "network/Transport/NetworkThread.hpp"

namespace server {

/// Adds every server system to `loop` and `world`, in the order they must run:
///
///     frameStart  NetReceiveSystem      socket → ReceivedPacket / IncompatibleClient
///                 ConnectionSystem      CONNECT_REQUEST → session, ACCEPT / REJECT
///                 SessionReceiveSystem  other packets → session, ClientMessage
///                 TimeoutSystem         silent sessions → Disconnecting
///                 InputSystem           INPUT → the player's PlayerInput
///                 PlayerShipSystem      one ship per connected player
///     simulation  TickSystem            SimulationTick + 1, first of every step
///                 PlayerControlSystem   PlayerInput → ship Velocity
///                 game::MovementSystem  Position += Velocity × step
///                 PlayerBoundsSystem    ships kept inside the arena
///     frameEnd    SessionCleanupSystem  Disconnecting → PLAYER_LEFT, entity destroyed
///                 ReplicationSystem     ENTITY_SPAWN / ENTITY_DESTROY
///                 NetSendSystem         reliable messages, resends, heartbeats
///                 SnapshotSystem        SNAPSHOT of every networked entity, 30 Hz
///                 DiscardPacketsSystem  unhandled messages destroyed
///
/// Every system runs on the game thread; `socket` hands datagrams to and from
/// the network thread.
///
/// `main` and the tests both use it, so they always run the same server.
void installSystems(ecs::World& world, ecs::GameLoop& loop, network::NetworkThread& socket);

} // namespace server
