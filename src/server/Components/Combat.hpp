#pragma once

#include "game/Bydo.hpp"
#include "game/Components/Collision.hpp"
#include "game/Components/Motion.hpp"
#include "network/Protocol/Protocol.hpp"

#include <cstdint>
#include <random>

/// Combat as data: what shoots, what it fires, what hurts and what it is
/// worth. Systems read these components whatever the entity is (ship, Bydo,
/// later a boss or the Force), so new content is new data, not a new system.
namespace server {

/// What a weapon fires: everything needed to create the projectile entity.
struct Projectile {
    network::protocol::EntityKind kind{network::protocol::EntityKind::PlayerMissile};
    float speed{}; // World units per second
    float halfWidth{};
    float halfHeight{};
    game::LayerMask layer{game::layer::None};
    game::LayerMask hits{game::layer::None}; // Layers it damages
    std::uint8_t damage{1};
};

/// Anything that can shoot. `WeaponSystem` fires it when its `Trigger` is
/// pulled and its cooldown is over.
struct Weapon {
    Projectile projectile;
    float interval{}; // Seconds between two shots
    float cooldown{}; // Seconds before the next shot
    game::Position muzzle{}; // Where shots leave, from the shooter's centre
};

/// The decision to shoot, written by a "brain" system (the player's keys in
/// `PlayerControlSystem`, the aim of `BydoAimSystem`), read by `WeaponSystem`.
struct Trigger {
    bool pulled{};
    game::Velocity direction{.dx = 1.0F, .dy = 0.0F}; // Unit vector
};

/// Hurts what it collides with: every layer in its `Hitbox::collidesWith`
/// loses `amount` hit points (see `DamageSystem`).
struct Damage {
    std::uint8_t amount{1};
    bool diesOnHit{true}; // Missiles, and Bydos crashing into a ship
};

/// Team score earned when this entity is killed.
struct ScoreValue {
    std::uint32_t points{};
};

/// Marks a Bydo (enemy) and drives its wave.
struct Bydo {
    float baseY{}; // Centre line of its wave
    float age{}; // Seconds since it spawned
};

/// When the next Bydo appears, and the random source for where, on a
/// singleton entity. Seeded once: the same seed gives the same waves.
struct BydoSpawner {
    float untilNext{game::bydo::SpawnInterval}; // The players get a moment first
    std::minstd_rand random{0x52545950}; // "RTYP"
};

/// The shared team score, on a singleton entity. Sent in GAME_STATE.
struct TeamScore {
    std::uint32_t value{};
};

} // namespace server
