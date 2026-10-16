#pragma once

#include "network/Protocol/Protocol.hpp"
#include "network/Reliability/ReliableChannel.hpp"
#include "network/Transport/UdpSocket.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace server {

/// A connected player: one entity per session, created by `ConnectionSystem`.
///
/// A session is identified by the address *and* the token (§7.1): a packet
/// from the right address with the wrong token is dropped.
struct NetClient {
    network::Endpoint endpoint;
    std::uint32_t token{}; // Random, non-zero, unique among sessions
    std::uint8_t playerId{}; // 0..3, also the ship colour
    std::string name;
    network::ReliableChannel channel;
    network::ReliableChannel::Clock::time_point lastReceived; // For the 5 s timeout
    network::ReliableChannel::Clock::time_point lastSent; // For the 250 ms heartbeat
};

/// Marks a session to close at the end of the frame (`SessionCleanupSystem`),
/// which tells the other players with PLAYER_LEFT.
struct Disconnecting {
    network::protocol::LeaveReason reason{network::protocol::LeaveReason::Quit};
    /// Sent to the leaving client itself, when it is still there to hear it.
    std::optional<network::protocol::DisconnectReason> notify;
};

} // namespace server
