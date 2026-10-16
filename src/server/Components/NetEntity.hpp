#pragma once

#include "network/Protocol/Protocol.hpp"

#include <cstdint>
#include <optional>

namespace server {

/// An entity the clients know about: everything that appears in snapshots
/// and spawn/destroy events (docs/protocol.md §5.1).
///
/// The `netId` is distinct from the ECS `Entity`: the ECS handle is an
/// internal detail, and 16 bits are enough on the wire.
struct NetEntity {
    std::uint16_t netId{network::protocol::NoEntity};
    network::protocol::EntityKind kind{network::protocol::EntityKind::PlayerShip};
    std::uint8_t owner{network::protocol::NoPlayer}; // playerId, or NoPlayer
};

/// State of the netId allocator, on a singleton entity (see `allocateNetId`).
struct NetIdCounter {
    std::uint16_t next{0};
};

/// Number of fixed simulation steps since the server started, on a singleton
/// entity. Snapshots carry it as `serverTick`.
struct SimulationTick {
    std::uint32_t value{0};
};

/// The `SimulationTick` of the last SNAPSHOT sent, on a singleton entity.
struct LastSnapshot {
    std::optional<std::uint32_t> tick; // nullopt until the first one
};

} // namespace server
