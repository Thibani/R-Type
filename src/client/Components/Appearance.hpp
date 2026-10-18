#pragma once

#include "network/Protocol/Protocol.hpp"

#include <cstdint>

namespace client {

/// What an entity looks like. The render system needs only this and a
/// `game::Position`: it knows nothing about the network or the game rules.
enum class Look : std::uint8_t {
    PlayerShip,
    Bydo,
    PlayerMissile,
    EnemyMissile,
    Unknown, // A kind added after this client was built: drawn as a "?" (§5.2)
};

struct Appearance {
    Look look{Look::Unknown};
    std::uint8_t player{network::protocol::NoPlayer}; // 0-3: ship colour and P1-P4 tag
};

/// The look of a network entity kind.
[[nodiscard]]
constexpr Look lookOf(network::protocol::EntityKind kind) noexcept
{
    switch (kind) {
    case network::protocol::EntityKind::PlayerShip:
        return Look::PlayerShip;
    case network::protocol::EntityKind::Bydo:
        return Look::Bydo;
    case network::protocol::EntityKind::PlayerMissile:
        return Look::PlayerMissile;
    case network::protocol::EntityKind::EnemyMissile:
        return Look::EnemyMissile;
    }
    return Look::Unknown;
}

/// A background star (with a `game::Position` and a `game::Velocity`).
struct Star {
    float size{2.0F}; // World units
};

} // namespace client
