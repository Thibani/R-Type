#pragma once

#include "ecs/Loop/GameLoop.hpp"
#include "ecs/World/World.hpp"
#include "network/Transport/NetworkThread.hpp"
#include "server/AccountStore.hpp"
#include "server/FriendStore.hpp"
#include "server/MessageStore.hpp"
#include "server/ScoreStore.hpp"

namespace server {

/// Adds every server system to `loop`, in the order they must run. `world` holds the
/// sessions, whether their players are on the main page or in a game. Each game has
/// its own World and loop (`GameRoom`), which `RoomSystem` runs:
///
///     frameStart  NetReceiveSystem      network thread inbox → ReceivedPacket / IncompatibleClient
///                 ConnectionSystem      CONNECT_REQUEST → session, ACCEPT / REJECT
///                 SessionReceiveSystem  other packets → session, ClientMessage
///                 LoginSystem           AUTH_REQUEST → account checked, main page open
///                 RoomMessageSystem     LEAVE_GAME → main page, INPUT → the player's game
///                 TimeoutSystem         silent sessions → Disconnecting
///                 FriendSystem          friends' messages, who is online, saved once a second
///                 GroupSystem           group orders, countdown, game started, GROUP_STATE
///                 ChatSystem            chat and private messages, saved once a second
///                 LeaderboardSystem     LEADERBOARD_REQUEST → LEADERBOARD, saved once a second
///     frameEnd    SessionCleanupSystem  Disconnecting → PLAYER_LEFT, entity destroyed
///                 RoomSystem            one frame of each game, ended games scored and removed
///                 NetSendSystem         reliable messages, resends, heartbeats
///                 SnapshotSystem        SNAPSHOT of each game, 30 Hz
///                 DiscardPacketsSystem  unhandled messages destroyed
///
/// Every system runs on the game thread; `socket` hands datagrams to and from
/// the network thread.
///
/// `accounts`, `friends`, `messages` and `scores` must outlive `loop`.
///
/// `main` and the tests both use it, so they always run the same server.
void installSystems(ecs::World& world, ecs::GameLoop& loop, network::NetworkThread& socket,
    AccountStore& accounts, FriendStore& friends, MessageStore& messages, ScoreStore& scores);

/// In the loop of each game:
///
///     frameStart  InputSystem           INPUT → the player's PlayerInput
///                 PlayerShipSystem      one ship per connected player, respawn delay
///     simulation  installSimulation()   below
///     frameEnd    ReplicationSystem     ENTITY_SPAWN / ENTITY_DESTROY / PLAYER_DIED / GAME_STATE
///
/// The simulation alone (the World's own systems, at the fixed step), in order:
///
///     TickSystem            SimulationTick + 1, first of every step
///     PlayerControlSystem   PlayerInput → ship Velocity, FIRE → ship Trigger
///     BydoSpawnSystem       a Bydo from the right every 1.5 s, while ships exist
///     BydoMovementSystem    Bydo Velocity: left, in a wave
///     BydoAimSystem         Bydo Trigger: pulled, aimed at the nearest ship
///     WeaponSystem          pulled Trigger + Weapon ready → projectile
///     game::MovementSystem  Position += Velocity × step
///     game::ConfinementSystem ships (ConfinedToArena) kept inside the arena
///     game::ArenaExitSystem missiles and Bydos that left → despawn (OUT_OF_BOUNDS)
///     game::CollisionSystem overlapping hitboxes → CollisionEvent
///     DamageSystem          CollisionEvent + Damage → hits, kills, score, respawn timer
///     game::DespawnSystem   DespawnRequest → DespawnEvent, entity destroyed
///
/// Separate so the gameplay tests run exactly the server's simulation.
void installSimulation(ecs::World& world);

} // namespace server
