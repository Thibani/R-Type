#pragma once

#include "ecs/System/System.hpp"

namespace game {

/// Pushes back inside the arena every entity that has a `ConfinedToArena`
/// and went past an edge.
///
/// It runs right after `MovementSystem`: movement may take a ship past the
/// border, confinement puts it back on the border, in the same step.
///
/// When an entity is stopped by an edge, its velocity on that axis becomes 0
/// (if it has a `Velocity`). Clients extrapolate positions from velocities
/// between two snapshots: a velocity still pushing outwards would make them
/// draw the ship beyond the edge. The other axis is left untouched, so a ship
/// can slide along a wall.
class ConfinementSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace game
