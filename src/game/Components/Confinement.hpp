#pragma once

namespace game {

/// Keeps an entity entirely inside the arena (see `Arena.hpp`).
///
/// `Position` is the centre of the entity; the half extents say how far its
/// edges are from that centre, so the whole sprite stays visible:
///
///     x is kept in [halfWidth, arena::Width  - halfWidth]
///     y is kept in [halfHeight, arena::Height - halfHeight]
///
/// Player ships have it; missiles and enemies do not (they must be able to
/// leave the screen).
struct ConfinedToArena {
    float halfWidth{};
    float halfHeight{};
};

} // namespace game
