#pragma once

#include "ecs/System/System.hpp"
#include "network/Transport/UdpSocket.hpp"

#include <cstddef>

namespace server {

/// Drains the socket once per frame and turns every valid datagram into a
/// `ReceivedPacket` entity. Runs first in `frameStart()`.
///
/// Malformed datagrams are dropped and counted, never logged one by one: a
/// flood of garbage must not flood the logs too.
class NetReceiveSystem final : public ecs::System {
public:
    /// Upper bound of datagrams read per frame, so a flood cannot keep one
    /// frame from ever ending. The rest waits in the OS buffer.
    static constexpr std::size_t MaxDatagramsPerFrame = 1024;

    explicit NetReceiveSystem(network::UdpSocket& socket) noexcept;

    void update(ecs::World& world, const ecs::Time& time) override;

    /// Datagrams rejected by `decode()` since the start.
    [[nodiscard]]
    std::size_t droppedCount() const noexcept;

private:
    network::UdpSocket& _socket;
    std::size_t _dropped{0};
};

} // namespace server
