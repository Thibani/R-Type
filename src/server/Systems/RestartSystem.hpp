#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// RESTART from a player after GAME OVER → a new game for everyone
/// (`startNewGame`). Ignored while a game is running, so the repeats of the
/// same request do nothing. Runs in `frameStart()`, after `InputSystem`.
class RestartSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
