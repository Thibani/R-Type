#pragma once

#include "ecs/Loop/GameLoop.hpp"
#include "network/Transport/UdpSocket.hpp"

namespace server {

/// Adds every server system to `loop`, in the order they must run:
///
///     frameStart  NetReceiveSystem      socket → ReceivedPacket / IncompatibleClient
///                 ConnectionSystem      CONNECT_REQUEST → session, ACCEPT / REJECT
///                 SessionReceiveSystem  other packets → session, ClientMessage
///                 TimeoutSystem         silent sessions → Disconnecting
///     frameEnd    SessionCleanupSystem  Disconnecting → PLAYER_LEFT, entity destroyed
///                 NetSendSystem         reliable messages, resends, heartbeats
///                 DiscardPacketsSystem  unhandled messages destroyed
///
/// `main` and the tests both use it, so they always run the same server.
void installSystems(ecs::GameLoop& loop, network::UdpSocket& socket);

} // namespace server
