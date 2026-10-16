#pragma once

#include "ecs/System/System.hpp"
#include "network/Transport/UdpSocket.hpp"

#include <cstddef>
#include <unordered_map>

namespace server {

/// Drains the socket once per frame and turns every valid datagram into a
/// `ReceivedPacket` entity. Runs first in `frameStart()`.
///
/// Malformed datagrams are dropped and counted, never logged one by one: a
/// flood of garbage must not flood the logs too. A CONNECT_REQUEST of another
/// protocol version becomes an `IncompatibleClient` entity, to be rejected.
class NetReceiveSystem final : public ecs::System {
public:
    /// Upper bound of datagrams read per frame, so a flood cannot keep one
    /// frame from ever ending. The rest waits in the OS buffer.
    static constexpr std::size_t MaxDatagramsPerFrame = 1024;

    /// Datagrams accepted per address and per second; the rest is dropped (§7.1).
    static constexpr std::size_t MaxPacketsPerSecond = 120;

    explicit NetReceiveSystem(network::UdpSocket& socket) noexcept;

    void update(ecs::World& world, const ecs::Time& time) override;

    /// Datagrams rejected by `decode()` since the start.
    [[nodiscard]]
    std::size_t droppedCount() const noexcept;

    /// Datagrams dropped because their address sent too many this second.
    [[nodiscard]]
    std::size_t rateLimitedCount() const noexcept;

private:
    /// True if `from` may still send this second, and counts the datagram.
    bool withinRate(const network::Endpoint& from, float now);

    network::UdpSocket& _socket;
    std::size_t _dropped{0};
    std::size_t _rateLimited{0};

    // Datagrams per address in the current one-second window. Cleared every
    // second, so it holds at most the addresses heard from during one second.
    std::unordered_map<network::Endpoint, std::size_t> _packetsThisSecond;
    float _windowStart{0.0F};
};

} // namespace server
