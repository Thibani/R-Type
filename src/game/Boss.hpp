#pragma once

#include <cstdint>

/// The boss, shared by the server (which plays it) and the client (which
/// shows the distance to it and its health).
///
/// A boss comes every `Interval` seconds of play. While it is there, no new
/// wave arrives. It flies in, stops at `StopX`, moves up and down, and cycles
/// through its attacks, faster once below half health. Killing it is worth
/// `Points` and one life (up to `run::MaxLives`), and the next one is
/// `HealthPerDefeat` hit points tougher.
namespace game::boss {

/// Seconds of play between two bosses (the distance bar of the HUD).
inline constexpr float Interval = 90.0F;

inline constexpr float Speed = 120.0F; // While flying in
inline constexpr float StopX = 1600.0F;
inline constexpr float BobAmplitude = 260.0F; // Up and down around the centre
inline constexpr float BobFrequency = 0.6F; // Radians per second

inline constexpr float HalfWidth = 120.0F;
inline constexpr float HalfHeight = 180.0F;

/// Health of the first boss; each defeat adds `HealthPerDefeat`, up to
/// `MaxHealth` (one byte in SNAPSHOT).
inline constexpr std::uint8_t BaseHealth = 160;
inline constexpr std::uint8_t HealthPerDefeat = 30;
inline constexpr std::uint8_t MaxHealth = 255;

inline constexpr std::uint32_t Points = 5000;

/// Each attack lasts `AttackSeconds`, then the next one starts. Below half
/// health the boss is enraged: its weapons reload `EnragedFactor` times as
/// long (faster).
inline constexpr float AttackSeconds = 4.0F;
inline constexpr float EnragedFactor = 0.6F;

/// Fan: `FanShots` aimed missiles every `FanInterval` seconds.
inline constexpr std::uint8_t FanShots = 7;
inline constexpr float FanSpread = 0.18F;
inline constexpr float FanInterval = 0.9F;

/// Ring: `RingShots` missiles all around every `RingInterval` seconds.
inline constexpr std::uint8_t RingShots = 16;
inline constexpr float RingInterval = 1.4F;

/// Summon: `SummonCount` Bugs come out of the boss once per summon attack.
inline constexpr std::uint8_t SummonCount = 3;

inline constexpr float MissileSpeed = 330.0F;

} // namespace game::boss
