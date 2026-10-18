#pragma once

#include "ecs/Loop/GameLoop.hpp"
#include "ecs/World/World.hpp"
#include "network/Transport/NetworkThread.hpp"

namespace server {

/// Adds every server system to `loop` and `world`, in the order they must run:
///
///     frameStart  NetReceiveSystem      network thread inbox → ReceivedPacket / IncompatibleClient
///                 ConnectionSystem      CONNECT_REQUEST → session, ACCEPT / REJECT
///                 SessionReceiveSystem  other packets → session, ClientMessage
///                 TimeoutSystem         silent sessions → Disconnecting
///                 InputSystem           INPUT → the player's PlayerInput
///                 PlayerShipSystem      one ship per connected player, respawn delay
///     simulation  installSimulation()   below
///     frameEnd    SessionCleanupSystem  Disconnecting → PLAYER_LEFT, entity destroyed
///                 ReplicationSystem     ENTITY_SPAWN / ENTITY_DESTROY / PLAYER_DIED / GAME_STATE
///                 NetSendSystem         reliable messages, resends, heartbeats
///                 SnapshotSystem        SNAPSHOT of every networked entity, 30 Hz
///                 DiscardPacketsSystem  unhandled messages destroyed
///
/// Every system runs on the game thread; `socket` hands datagrams to and from
/// the network thread.
///
/// `main` and the tests both use it, so they always run the same server.
void installSystems(ecs::World& world, ecs::GameLoop& loop, network::NetworkThread& socket);

/// The simulation alone (the World's own systems, at the fixed step), in order:
///
///     TickSystem            SimulationTick + 1, first of every step
///     PlayerControlSystem   PlayerInput → ship Velocity
///     PlayerFireSystem      FIRE held → player missile, every 0.25 s
///     BydoSpawnSystem       a Bydo from the right every 1.5 s, while ships exist
///     BydoMovementSystem    Bydo Velocity: left, in a wave
///     BydoFireSystem        each Bydo fires at the nearest ship every 2.5 s
///     game::MovementSystem  Position += Velocity × step
///     PlayerBoundsSystem    ships kept inside the arena
///     game::ArenaExitSystem missiles and Bydos that left → despawn (OUT_OF_BOUNDS)
///     game::CollisionSystem overlapping hitboxes → CollisionEvent
///     DamageSystem          CollisionEvent → hits, kills, score, respawn timer
///     game::DespawnSystem   DespawnRequest → DespawnEvent, entity destroyed
///
/// Separate so the gameplay tests run exactly the server's simulation.
void installSimulation(ecs::World& world);

} // namespace server
