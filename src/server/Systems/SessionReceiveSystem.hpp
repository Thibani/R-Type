#pragma once

#include "ecs/System/System.hpp"

#include <cstddef>

namespace server {

/// Hands every received packet to its session. Runs in `frameStart()`, after
/// `ConnectionSystem` (which already took the CONNECT_REQUESTs).
///
///  - unknown address, or wrong token for that address (§7 rule 10): dropped;
///  - otherwise the session's `ReliableChannel` processes it: acknowledgements,
///    duplicates, reliable ordering;
///  - HEARTBEAT only keeps the session alive; DISCONNECT closes it;
///  - game payloads (INPUT...) become `ClientMessage` entities for gameplay.
class SessionReceiveSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;

    /// Packets dropped because no session matched their address and token.
    [[nodiscard]]
    std::size_t unknownSenderCount() const noexcept;

private:
    std::size_t _unknownSenders{0};
};

} // namespace server
