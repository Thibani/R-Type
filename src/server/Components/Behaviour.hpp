#pragma once

#include "server/Components/Combat.hpp"

#include <cstdint>

/// How enemies move and aim, as data. Each component is read by one small
/// "brain" system that only sets a `Velocity` or a `Trigger`; an enemy is the
/// combination of the behaviours it is given (see `Spawn.hpp`).
namespace server {

/// Marks an enemy, whatever its kind: what the players have to shoot down.
struct Enemy { };

/// Flies left at `speed` in a wave around `baseY` (`WaveFlightSystem`):
///
///     y = baseY + amplitude × sin(frequency × age)
struct WaveFlight {
    float baseY{}; // Centre line of the wave
    float speed{}; // Units per second towards the left
    float amplitude{}; // Units above and below baseY
    float frequency{}; // Radians per second
    float age{}; // Seconds since it spawned
};

/// Flies left at `speed` and steers up or down towards the nearest ship
/// (`SeekerSystem`). It never turns back: if it misses, it leaves on the left.
struct Seeker {
    float speed{}; // Units per second towards the left
    float maxVerticalSpeed{}; // Units per second
    float verticalAcceleration{}; // Units per second²
};

/// Points its `Trigger` at the nearest ship and pulls it while it is on
/// screen (`AimSystem`). Its `Weapon` decides how often it actually fires.
struct AimsAtShips { };

/// Flies left at `speed` while looping around a centre line
/// (`CircleFlightSystem`): it draws loops of `radius` units, `loopSpeed`
/// radians per second.
struct CircleFlight {
    float speed{}; // Drift towards the left, units per second
    float radius{};
    float loopSpeed{}; // Radians per second
    float angle{}; // Where it is on its loop, radians
};

/// Walks left on the floor, jumping each time it lands (`HopSystem`).
struct Hop {
    float floorY{}; // Centre height when standing on the floor
    float jumpSpeed{}; // Upwards, units per second
    float gravity{}; // Units per second²
};

/// Flies left at `speed` until `stopX`, stays there `holdSeconds` (forever if
/// 0) moving up and down, then leaves on the left (`HoverSystem`).
struct Hover {
    float stopX{};
    float speed{}; // Units per second, coming in and leaving
    float holdSeconds{}; // 0: never leaves
    float bobAmplitude{}; // Units up and down while holding
    float bobFrequency{}; // Radians per second
    float held{}; // Seconds spent holding so far
};

/// When killed, fires `shots` projectiles all around (`DeathBurstSystem`).
struct BurstOnDeath {
    Projectile projectile;
    std::uint8_t shots{};
};

/// Marks a boss: while one is in the game, no new wave arrives.
struct Boss {
    std::uint8_t maxHealth{}; // Its health when it arrived
};

/// What a boss does, attack after attack (`BossSystem`).
enum class BossAttack : std::uint8_t {
    Fan, // Aimed fan of missiles
    Ring, // Missiles all around
    Summon, // Bugs come out of it
};

/// The attack a boss is doing, and for how long still.
struct AttackCycle {
    BossAttack current{BossAttack::Fan};
    float timeLeft{};
    bool summoned{}; // This Summon attack already brought its Bugs
    bool enraged{}; // Below half health: weapons reload faster
};

} // namespace server
