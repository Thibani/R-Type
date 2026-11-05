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
///                 RestartSystem         RESTART after GAME OVER → a new game
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
///     MatchSystem           no life and no ship left → GAME OVER; nobody left → new game
///     PlayerControlSystem   PlayerInput → ship Velocity, FIRE → ship Trigger
///     ProgressSystem        seconds played (while ships exist) → level of the run
///     BossSystem            boss arrival (every 90 s of play), its attacks, its defeat
///     WaveSystem            enemies and formations from the right, faster each level
///     WaveFlightSystem      WaveFlight Velocity: left, in a wave (Bydo, PataPata, Cancer)
///     SeekerSystem          Seeker Velocity: left, steering towards a ship (Bug)
///     CircleFlightSystem    CircleFlight Velocity: loops while drifting left (Spinner)
///     HopSystem             Hop Velocity: jumps along the floor (Hopper)
///     HoverSystem           Hover Velocity: stops, holds, leaves (Gunner, boss)
///     AimSystem             AimsAtShips Trigger: pulled on screen, aimed at the nearest ship
///     WeaponSystem          pulled Trigger + Weapon ready → projectile
///     game::MovementSystem  Position += Velocity × step
///     game::ConfinementSystem ships (ConfinedToArena) kept inside the arena
///     game::ArenaExitSystem missiles and enemies that left → despawn (OUT_OF_BOUNDS)
///     game::CollisionSystem overlapping hitboxes → CollisionEvent
///     DamageSystem          CollisionEvent + Damage → hits, kills, score
///     LivesSystem           ship killed → one life less, respawn timer or Eliminated
///     DeathBurstSystem      BurstOnDeath killed → projectiles all around (Mine)
///     game::DespawnSystem   DespawnRequest → DespawnEvent, entity destroyed
///
/// Separate so the gameplay tests run exactly the server's simulation.
void installSimulation(ecs::World& world);

} // namespace server
