#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Moves logged-in players between the main page and the game: JOIN_GAME and
/// LEAVE_GAME (docs/protocol.md §8.4). Runs in `frameStart()`, after `LoginSystem`
/// and before `PlayerShipSystem`, which creates and removes the ships.
///
/// A place freed by LEAVE_GAME can only be taken from the next frame on. Until
/// then, `PlayerShipSystem` has removed the old ship, and a newcomer cannot get it.
class JoinSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
