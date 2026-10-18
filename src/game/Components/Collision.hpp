#pragma once

#include <cstdint>

namespace game {

/// A set of collision layers, as bit flags.
using LayerMask = std::uint8_t;

/// What an entity is, for collisions. An entity belongs to one layer.
namespace layer {

inline constexpr LayerMask None = 0;
inline constexpr LayerMask PlayerShip = 1U << 0U;
inline constexpr LayerMask PlayerMissile = 1U << 1U;
inline constexpr LayerMask Enemy = 1U << 2U;
inline constexpr LayerMask EnemyMissile = 1U << 3U;

} // namespace layer

/// An axis-aligned box centred on the entity's `Position`, used for
/// collisions. Without a `Hitbox`, an entity never collides.
///
/// `layer` says what the entity is; `collidesWith` says which layers it can
/// hit. Two entities collide when their boxes overlap and at least one of
/// them wants to hit the other:
///
///     // A player missile hits enemies, never ships nor other missiles
///     Hitbox{.halfWidth = 8, .halfHeight = 2,
///            .layer = layer::PlayerMissile, .collidesWith = layer::Enemy};
struct Hitbox {
    float halfWidth{};
    float halfHeight{};
    LayerMask layer{layer::None};
    LayerMask collidesWith{layer::None};
};

} // namespace game
