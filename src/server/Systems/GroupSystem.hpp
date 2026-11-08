#pragma once

#include "ecs/System/System.hpp"
#include "network/Protocol/Packets.hpp"
#include "server/FriendStore.hpp"
#include "server/Groups.hpp"

#include <cstddef>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace server {

class OnlinePlayers;

/// Groups on the wire: the orders of their members, the countdown, and GROUP_STATE
/// (docs/protocol.md §8.7). Runs in `frameStart()`, after `FriendSystem`. The rules
/// themselves are in `Groups`.
///
/// Each player is told their group's state whenever it differs from the last one they
/// were sent: every change, whatever caused it, reaches everyone it concerns.
class GroupSystem final : public ecs::System {
public:
    /// Wrong codes per player and per window: 1 billion possible codes stay out of reach.
    static constexpr std::size_t MaxWrongCodes = 5; // A choice, not a measure
    static constexpr float WrongCodeWindowSeconds = 30.0F;

    explicit GroupSystem(const FriendStore& friends);

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    void updateOnline(ecs::World& world, Groups& groups);
    void handleMessages(ecs::World& world, Groups& groups, float now);
    void handleCode(Groups& groups, const OnlinePlayers& online, const std::string& name,
        network::protocol::GroupCodeAction action);
    void handleJoin(Groups& groups, const OnlinePlayers& online, const std::string& name,
        const std::string& code, float now);
    void handleInvite(Groups& groups, const OnlinePlayers& online, const std::string& name,
        const std::string& invited);
    void runCountdowns(ecs::World& world, Groups& groups, const ecs::Time& time);
    void sendStates(ecs::World& world, Groups& groups);
    [[nodiscard]]
    std::string newCode(const Groups& groups);

    const FriendStore& _friends;
    std::set<std::string> _online; // Names, as of the last frame
    std::map<std::string, network::protocol::GroupState> _lastSent;
    std::map<std::string, std::vector<float>> _wrongCodes; // Times, within the window
    // Non-deterministic source, as for the session tokens: a code must not be guessable
    std::random_device _random;
};

} // namespace server
