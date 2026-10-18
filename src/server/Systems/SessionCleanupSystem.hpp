#pragma once

#include "ecs/System/System.hpp"
#include "network/Transport/NetworkThread.hpp"

namespace server {

/// Closes the sessions marked `Disconnecting` this frame: tells the client
/// why when it can still hear it, destroys its entity, and tells every other
/// player with a reliable PLAYER_LEFT. The game goes on (§8.2).
///
/// Runs first in `frameEnd()`, so PLAYER_LEFT leaves in the same frame.
class SessionCleanupSystem final : public ecs::System {
public:
    explicit SessionCleanupSystem(network::NetworkThread& socket) noexcept;

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    network::NetworkThread& _socket;
};

} // namespace server
