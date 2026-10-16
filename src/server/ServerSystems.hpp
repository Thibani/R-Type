#pragma once

#include "ecs/Loop/GameLoop.hpp"
#include "ecs/World/World.hpp"
#include "network/Transport/UdpSocket.hpp"

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
///                 NetSendSystem         reliable messages, resends, heartbeats
///                 DiscardPacketsSystem  unhandled messages destroyed
///
/// `main` and the tests both use it, so they always run the same server.
void installSystems(ecs::World& world, ecs::GameLoop& loop, network::UdpSocket& socket);

} // namespace server
