#pragma once

#include "ecs/Entity/Entity.hpp"
#include "network/Protocol/Protocol.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace client {

/// An entity the server told us about (ENTITY_SPAWN or SNAPSHOT).
struct NetEntity {
    std::uint16_t netId{};
    network::protocol::EntityKind kind{network::protocol::EntityKind::PlayerShip};
    std::uint8_t owner{network::protocol::NoPlayer};
    std::uint8_t health{};
};

/// Singleton: which local entity stands for which netId (§5.1).
struct NetRegistry {
    std::unordered_map<std::uint16_t, ecs::Entity> entities;
    /// netId → time of its ENTITY_DESTROY: ignored in snapshots for 5 s (§5.1).
    std::unordered_map<std::uint16_t, float> destroyedAt;
    /// Newest snapshot tick applied: older snapshots are ignored (§4.3).
    std::optional<std::uint32_t> newestTick;
};

} // namespace client
