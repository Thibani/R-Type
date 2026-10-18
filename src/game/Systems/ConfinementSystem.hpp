#pragma once

#include "ecs/System/System.hpp"

namespace game {

/// Pushes back inside the arena every entity that has a `ConfinedToArena`
/// and went past an edge.
///
/// It runs after `MovementSystem`: movement may take a ship past the border,
/// confinement then puts it back on the border, in the same frame. Only the
/// position is corrected; the velocity is left to whoever controls it.
class ConfinementSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace game
