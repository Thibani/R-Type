#pragma once

#include <cstdint>

/// Player ship rules, shared by the server (which applies them) and the
/// client (which will predict them, #80): both must use the same numbers.
namespace game::ship {

/// World units per second, in any direction (diagonals included).
/// The arena is 1920 units wide: about 4 seconds to cross it.
inline constexpr float Speed = 480.0F;

/// Half of the ship's size, in world units: the ship's centre stays at least
/// this far from the arena edges, so the whole ship is always visible.
inline constexpr float HalfWidth = 32.0F;
inline constexpr float HalfHeight = 16.0F;

/// Hit points of a new ship: the 3 hearts shown to the player.
inline constexpr std::uint8_t MaxHealth = 3;

/// A destroyed ship comes back in its lane after this delay, with full health.
inline constexpr float RespawnDelay = 3.0F;

/// Holding FIRE shoots one missile every `FireCooldown` seconds.
inline constexpr float FireCooldown = 0.25F;
inline constexpr float MissileSpeed = 1200.0F;
inline constexpr float MissileHalfWidth = 12.0F;
inline constexpr float MissileHalfHeight = 4.0F;

} // namespace game::ship
