#pragma once

#include "ecs/System/System.hpp"

namespace client {

class ServerConnection;

/// Frame start: reads the server's packets and applies them to the world
/// (`Replication.hpp`). Follows the connection's status: CONNECT_ACCEPT starts
/// the game, a failure goes back to the connection screen with its reason.
class NetReceiveSystem final : public ecs::System {
public:
    explicit NetReceiveSystem(ServerConnection& connection) noexcept;

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    ServerConnection& _connection;
};

} // namespace client
