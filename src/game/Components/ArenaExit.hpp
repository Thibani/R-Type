#pragma once

namespace game {

/// Despawns an entity once it has left the arena (see `Arena.hpp`).
///
/// The entity is despawned, with the reason `OutOfBounds`, when its centre
/// is more than `margin` units outside the arena on any side:
///
///     x < -margin  or  x > arena::Width  + margin
///     y < -margin  or  y > arena::Height + margin
///
/// The margin leaves room for the sprite to slide fully off screen before
/// disappearing, and for enemies to spawn just outside the right edge and fly
/// in. Missiles and enemies have it; player ships have `ConfinedToArena`
/// instead and never leave.
struct DespawnOutsideArena {
    /// Enough for any R-Type sprite to be fully hidden.
    static constexpr float DefaultMargin = 128.0F;

    float margin{DefaultMargin};
};

} // namespace game
