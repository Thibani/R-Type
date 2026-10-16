#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Keeps player ships inside the arena. Runs in the simulation, right after
/// `game::MovementSystem`.
///
/// A ship pushed against an edge stops there, and its velocity on that axis
/// becomes 0: clients extrapolate positions from velocities between
/// snapshots, so a non-zero velocity would make them draw it beyond the edge.
///
/// Only ships: Bydos and missiles enter and leave through the edges.
class PlayerBoundsSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
