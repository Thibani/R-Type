#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

/// Rules of the gadgets, shared by the server and the client: what killed
/// enemies drop, and the Force, the pod that flies and shoots with a ship.
/// Speeds are in world units per second, sizes are half extents.
namespace game {

/// What an enemy can leave behind when it is destroyed. Pickups drift left,
/// and leave the arena if nobody takes them.
namespace pickup {

inline constexpr float Speed = 120.0F;
inline constexpr float HalfSize = 20.0F;

/// Chance, per destroyed enemy, of each drop (at most one per enemy).
inline constexpr float RepairChance = 0.08F;
inline constexpr float ForceChance = 0.05F;

/// Taking a Force capsule when the ship already has one gives points instead.
inline constexpr std::uint32_t ForceBonusPoints = 500;

} // namespace pickup

/// The Force: an armoured pod attached to a ship, in one of 4 slots around it.
/// It fires with the ship, away from it, and stops enemy missiles. The POD key
/// moves it to the next slot. It is lost with the ship.
namespace force {

inline constexpr float HalfWidth = 25.0F;
inline constexpr float HalfHeight = 20.0F;

/// Where the pod is, around the ship. Each press of POD moves it to the next.
enum class Slot : std::uint8_t {
    Front,
    Above,
    Behind,
    Below,
};
inline constexpr std::size_t SlotCount = 4;

/// The slot after `slot`, turning around the ship.
[[nodiscard]]
constexpr Slot next(Slot slot) noexcept
{
    return static_cast<Slot>((static_cast<std::size_t>(slot) + 1) % SlotCount);
}

/// A unit vector from the ship towards `slot`: where the pod is, and where it
/// fires (y grows downwards).
struct Direction {
    float x{};
    float y{};
};
[[nodiscard]]
constexpr Direction outwards(Slot slot) noexcept
{
    constexpr std::array<Direction, SlotCount> directions{
        Direction{.x = 1.0F, .y = 0.0F},
        Direction{.x = 0.0F, .y = -1.0F},
        Direction{.x = -1.0F, .y = 0.0F},
        Direction{.x = 0.0F, .y = 1.0F},
    };
    return directions.at(static_cast<std::size_t>(slot));
}

/// Distance between the centres of the ship and the pod, along each axis:
/// the pod touches the ship (ship 32 × 16, pod 25 × 20, a few units apart).
inline constexpr float GapX = 64.0F;
inline constexpr float GapY = 42.0F;

/// It fires while the ship's FIRE is held, a little slower than the ship.
inline constexpr float FireCooldown = 0.35F;

} // namespace force

} // namespace game
