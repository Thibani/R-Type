#pragma once

#include "ecs/System/System.hpp"
#include "server/ScoreStore.hpp"

namespace server {

/// What players in a game send: LEAVE_GAME brings them back to the main page, and each
/// INPUT goes to the World of their game, for its `InputSystem`. Runs in `frameStart()`,
/// after `LoginSystem`.
class RoomMessageSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

/// Runs one frame of each game, then removes those with no player left. Runs in
/// `frameEnd()`, after `SessionCleanupSystem`, and before `NetSendSystem`, which sends
/// the events the games have just queued.
///
/// A game ends when its last player leaves: its team score then enters the leaderboard,
/// under the names of the accounts that started it (docs/protocol.md §8.8).
class RoomSystem final : public ecs::System {
public:
    explicit RoomSystem(ScoreStore& scores) noexcept;

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    ScoreStore& _scores;
};

} // namespace server
