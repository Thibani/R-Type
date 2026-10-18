#pragma once

#include "ecs/System/System.hpp"

namespace client {

class ServerConnection;

/// Frame end: everything that goes to the server. Starts or leaves a session
/// when the interface asks for it, sends the recorded inputs (INPUT), then
/// lets the connection retry, send heartbeats and detect timeouts.
class NetSendSystem final : public ecs::System {
public:
    explicit NetSendSystem(ServerConnection& connection) noexcept;

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    ServerConnection& _connection;
};

} // namespace client
