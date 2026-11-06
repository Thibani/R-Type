#pragma once

#include "ecs/System/System.hpp"
#include "server/AccountStore.hpp"
#include "server/FriendStore.hpp"

#include <set>
#include <string>

namespace server {

/// Friends: FRIEND_REQUEST, FRIEND_ANSWER, FRIEND_REMOVE, and who is online
/// (docs/protocol.md §8.5). Runs in `frameStart()`, after `LoginSystem` and
/// `TimeoutSystem`, which tell who has just arrived and who is leaving.
///
/// Who is online comes from comparing the logged-in sessions with those of the
/// last frame, so every way of leaving (DISCONNECT, timeout, kick) counts.
class FriendSystem final : public ecs::System {
public:
    static constexpr float SecondsBetweenSaves = 1.0F;

    FriendSystem(const AccountStore& accounts, FriendStore& friends) noexcept;

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    void updateOnline(ecs::World& world);
    void handleMessages(ecs::World& world);
    void saveWhenDue(const ecs::Time& time);

    const AccountStore& _accounts;
    FriendStore& _friends;
    std::set<std::string> _online; // Names, as of the last frame
    float _lastSave{0.0F};
};

} // namespace server
