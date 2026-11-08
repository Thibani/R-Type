#pragma once

#include "ecs/System/System.hpp"
#include "server/ScoreStore.hpp"

namespace server {

/// The leaderboard: answers LEADERBOARD_REQUEST with the 10 best games of the ranking
/// asked for, and the rank of the player's last game there (docs/protocol.md §8.8).
/// Saves the scores at most once per second. Runs in `frameStart()`, after `LoginSystem`.
///
/// The games themselves enter the store when they end, in `RoomSystem`.
class LeaderboardSystem final : public ecs::System {
public:
    static constexpr float SecondsBetweenSaves = 1.0F;

    explicit LeaderboardSystem(ScoreStore& scores) noexcept;

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    void saveWhenDue(const ecs::Time& time);

    ScoreStore& _scores;
    float _lastSave{0.0F};
};

} // namespace server
