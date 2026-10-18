#pragma once

#include "ecs/System/System.hpp"
#include "network/Transport/UdpSocket.hpp"

#include <chrono>

namespace server {

/// Sends each session what its `ReliableChannel` has due (new reliable
/// messages and resends), or a HEARTBEAT when nothing was sent for 250 ms so
/// the client keeps receiving acknowledgements and knows the server is alive
/// (§8.2). Runs in `frameEnd()`, after the systems that queue messages.
class NetSendSystem final : public ecs::System {
public:
    static constexpr std::chrono::milliseconds HeartbeatInterval{250};

    explicit NetSendSystem(network::UdpSocket& socket) noexcept;

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    network::UdpSocket& _socket;
};

} // namespace server
