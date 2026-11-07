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
    PataPata,
    Bug,
    Cancer,
    Cruiser,
    Hopper,
    Spinner,
    Gunner,
    Mine,
    Boss,
    Force,
    ForceCapsule,
    RepairKit,
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
    case network::protocol::EntityKind::PataPata:
        return Look::PataPata;
    case network::protocol::EntityKind::Bug:
        return Look::Bug;
    case network::protocol::EntityKind::Cancer:
        return Look::Cancer;
    case network::protocol::EntityKind::Cruiser:
        return Look::Cruiser;
    case network::protocol::EntityKind::Hopper:
        return Look::Hopper;
    case network::protocol::EntityKind::Spinner:
        return Look::Spinner;
    case network::protocol::EntityKind::Gunner:
        return Look::Gunner;
    case network::protocol::EntityKind::Mine:
        return Look::Mine;
    case network::protocol::EntityKind::Boss:
        return Look::Boss;
    case network::protocol::EntityKind::Force:
        return Look::Force;
    case network::protocol::EntityKind::ForceCapsule:
        return Look::ForceCapsule;
    case network::protocol::EntityKind::RepairKit:
        return Look::RepairKit;
    }
    return Look::Unknown;
}

/// A background star (with a `game::Position` and a `game::Velocity`).
struct Star {
    float size{2.0F}; // World units
};

} // namespace client
