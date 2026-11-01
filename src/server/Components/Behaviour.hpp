#pragma once

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

} // namespace server
