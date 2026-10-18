#pragma once

#include <cstdint>

/// Bydo (enemy) rules, shared by the server and the client.
namespace game::bydo {

/// Units per second towards the left: about 10 s to cross the arena.
inline constexpr float Speed = 180.0F;

/// Bydos fly in a wave: `y = baseY + WaveAmplitude × sin(WaveFrequency × age)`.
inline constexpr float WaveAmplitude = 80.0F;
inline constexpr float WaveFrequency = 2.0F; // Radians per second

inline constexpr float HalfWidth = 24.0F;
inline constexpr float HalfHeight = 20.0F;

/// One player missile kills a Bydo.
inline constexpr std::uint8_t MaxHealth = 1;

/// Team score earned for each Bydo killed.
inline constexpr std::uint32_t Points = 100;

/// A new Bydo enters from the right every `SpawnInterval` seconds.
inline constexpr float SpawnInterval = 1.5F;

/// Each Bydo fires at the nearest ship every `FireInterval` seconds.
inline constexpr float FireInterval = 2.5F;
inline constexpr float MissileSpeed = 360.0F;
inline constexpr float MissileHalfSize = 6.0F;

} // namespace game::bydo
