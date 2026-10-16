#pragma once

#include "ecs/System/System.hpp"
#include "network/Transport/UdpSocket.hpp"

#include <cstdint>
#include <random>

namespace server {

/// Answers CONNECT_REQUEST (docs/protocol.md §8.1). Runs in `frameStart()`,
/// right after `NetReceiveSystem`.
///
///  - other protocol version, empty name, or 4 players already: CONNECT_REJECT;
///  - otherwise: a new player entity with a `NetClient`, a CONNECT_ACCEPT,
///    and reliable PLAYER_JOINED messages so that everyone knows everyone;
///  - a request from an address that already has a session gets the same
///    CONNECT_ACCEPT again, since the first one may have been lost.
///
/// It consumes every CONNECT_REQUEST, so later systems never see one.
class ConnectionSystem final : public ecs::System {
public:
    static constexpr std::uint8_t TickRate = 60; // §9, matches GameLoop's default step
    static constexpr std::uint8_t SnapshotRate = 30; // §9

    explicit ConnectionSystem(network::UdpSocket& socket);

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    /// A random token, non-zero and not used by another session (§7.1).
    [[nodiscard]]
    std::uint32_t newToken(ecs::World& world);

    network::UdpSocket& _socket;
    // Non-deterministic source: tokens must not be guessable. libstdc++, libc++
    // and MSVC back it with the OS's secure generator (getrandom, RtlGenRandom).
    std::random_device _random;
};

} // namespace server
