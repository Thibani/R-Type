#pragma once

#include "network/Protocol/Packets.hpp"
#include "network/Transport/UdpSocket.hpp"

namespace server {

/// A valid packet received this frame, stored on its own short-lived entity.
///
/// `NetReceiveSystem` creates one entity per packet. The systems that handle
/// packets (connection, input...) query this component, act on it and destroy
/// the entity. They run in `frameStart()`, after `NetReceiveSystem`: a frame
/// can run zero simulation steps, so simulation systems might never see it.
/// Whatever is left is destroyed by `DiscardPacketsSystem` at the end of the frame.
struct ReceivedPacket {
    network::Endpoint from;
    network::protocol::Packet packet;
};

} // namespace server
