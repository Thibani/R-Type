#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Keeps one ship per connected player: creates the ship of a player who has
/// none, and destroys the ships whose player left. Runs at the end of
/// `frameStart()`, after the sessions were updated.
///
/// A ship is its own entity: `PlayerShip`, `Position`, `Velocity` and a
/// `NetEntity`. It is not the player's entity, because a ship can die while
/// its player stays connected (#59).
///
/// shortcut: a player without a ship gets a new one at once; respawn rules
/// (delay, lives) come with damage and death (#59).
class PlayerShipSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
