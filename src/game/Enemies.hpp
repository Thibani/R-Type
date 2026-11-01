#pragma once

#include <cstdint>

/// Rules of the enemies other than the Bydo (see `Bydo.hpp`), shared by the
/// server and the client. Speeds are in world units per second, angles in
/// radians, sizes are half extents of the hitbox.
///
/// | Enemy    | Moves                        | Shoots                     | HP | Points |
/// |----------|------------------------------|----------------------------|---:|-------:|
/// | Bydo     | left, gentle wave            | 1 missile, aimed           |  1 |    100 |
/// | PataPata | left, fast and wide wave     | never (rams)               |  1 |     50 |
/// | Bug      | left, steers towards a ship  | never (rams)               |  1 |    150 |
/// | Cancer   | slowly left, small wave      | 3 missiles, aimed fan      |  3 |    300 |
/// | Cruiser  | very slowly left             | 5 missiles, aimed fan      | 15 |   1000 |
namespace game {

/// Small spinning drone: fast, erratic, harmless unless it rams a ship.
namespace patapata {

inline constexpr float Speed = 300.0F;
inline constexpr float WaveAmplitude = 140.0F;
inline constexpr float WaveFrequency = 4.0F; // Radians per second
inline constexpr float HalfWidth = 20.0F;
inline constexpr float HalfHeight = 20.0F;
inline constexpr std::uint8_t MaxHealth = 1;
inline constexpr std::uint32_t Points = 50;

} // namespace patapata

/// Insect that flies left and steers up or down towards the nearest ship.
/// It never turns back, so it always leaves on the left if it misses.
namespace bug {

inline constexpr float Speed = 260.0F;
inline constexpr float MaxVerticalSpeed = 240.0F;
inline constexpr float VerticalAcceleration = 600.0F; // Units per second²
inline constexpr float HalfWidth = 22.0F;
inline constexpr float HalfHeight = 12.0F;
inline constexpr std::uint8_t MaxHealth = 1;
inline constexpr std::uint32_t Points = 150;

} // namespace bug

/// Armoured crab: slow, takes 3 hits, fires a fan of 3 missiles at a ship.
namespace cancer {

inline constexpr float Speed = 120.0F;
inline constexpr float WaveAmplitude = 40.0F;
inline constexpr float WaveFrequency = 1.0F; // Radians per second
inline constexpr float HalfWidth = 28.0F;
inline constexpr float HalfHeight = 24.0F;
inline constexpr std::uint8_t MaxHealth = 3;
inline constexpr std::uint32_t Points = 300;

inline constexpr float FireInterval = 2.2F;
inline constexpr std::uint8_t Shots = 3;
inline constexpr float Spread = 0.35F; // Between two missiles of the fan
inline constexpr float MissileSpeed = 300.0F;

} // namespace cancer

/// Heavy battleship: crosses the arena in about 30 s, takes 15 hits, fires a
/// fan of 5 missiles. Ramming it destroys the ship, not the cruiser.
namespace cruiser {

inline constexpr float Speed = 70.0F;
inline constexpr float HalfWidth = 54.0F;
inline constexpr float HalfHeight = 54.0F;
inline constexpr std::uint8_t MaxHealth = 15;
inline constexpr std::uint32_t Points = 1000;
inline constexpr std::uint8_t RamDamage = 3; // A full-health ship

inline constexpr float FireInterval = 1.8F;
inline constexpr std::uint8_t Shots = 5;
inline constexpr float Spread = 0.25F; // Between two missiles of the fan
inline constexpr float MissileSpeed = 280.0F;

} // namespace cruiser

} // namespace game
