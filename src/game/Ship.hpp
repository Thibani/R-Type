#pragma once

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

} // namespace game::ship
