#pragma once

#include "ecs/Entity/Entity.hpp"
#include "network/Protocol/Packets.hpp"

namespace server {

/// A game payload (INPUT...) from a connected player, on its own short-lived
/// entity, created by `SessionReceiveSystem` once the session was checked and
/// duplicates removed. Gameplay systems consume it in `frameStart()`, like
/// `ReceivedPacket`; leftovers are destroyed at the end of the frame.
struct ClientMessage {
    ecs::Entity client; // The entity holding the sender's NetClient
    network::protocol::Payload payload;
};

} // namespace server
