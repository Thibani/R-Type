#pragma once

#include "network/Crypto/Crypto.hpp"
#include "network/Protocol/Protocol.hpp"
#include "network/Reliability/ReliableChannel.hpp"
#include "network/Transport/UdpSocket.hpp"

#include <cstdint>
#include <optional>
#include <set>
#include <string>

namespace server {

/// A connected player: one entity per session, created by `ConnectionSystem`.
///
/// A session is identified by the address *and* the token (§7.1): a packet
/// from the right address with the wrong token is dropped.
struct NetClient {
    network::Endpoint endpoint;
    std::uint32_t token{}; // Random, non-zero, unique among sessions
    std::string name;
    network::ReliableChannel channel;
    network::crypto::SessionCipher cipher; // Seals and opens every packet of the session
    network::crypto::PublicKey clientPublicKey; // A request with another key is not this client
    network::crypto::PublicKey serverPublicKey; // Repeated in every CONNECT_ACCEPT
    network::ReliableChannel::Clock::time_point connectedAt; // For the login timeout
    bool loginRequested{false}; // One attempt per connection
    network::ReliableChannel::Clock::time_point lastReceived; // For the 5 s timeout
    network::ReliableChannel::Clock::time_point lastSent; // For the 250 ms heartbeat
    std::set<std::uint16_t> knownEntities; // netIds sent in ENTITY_SPAWN, not destroyed since
    std::optional<std::uint32_t> knownScore; // Score of the last GAME_STATE it was sent
};

/// On a `NetClient` that has logged in to its account: it is on the main page.
/// Before that, the session only carries the login, encrypted.
struct LoggedIn { };

/// On a logged-in player who entered the game (`JoinSystem`): only those get a
/// ship and the game's events. The place in the game exists only while playing.
struct InGame {
    std::uint8_t playerId{}; // 0..3, also the ship colour
};

/// Marks a session to close at the end of the frame (`SessionCleanupSystem`),
/// which tells the other players with PLAYER_LEFT.
struct Disconnecting {
    network::protocol::LeaveReason reason{network::protocol::LeaveReason::Quit};
    /// Sent to the leaving client itself, when it is still there to hear it.
    std::optional<network::protocol::DisconnectReason> notify;
};

} // namespace server
