#pragma once

#include "ecs/System/System.hpp"
#include "server/FriendStore.hpp"
#include "server/MessageStore.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace server {

/// The chat: CHAT_SEND, PRIVATE_SEND, and the private messages waiting for a player
/// who comes online (docs/protocol.md §8.6). Runs in `frameStart()`, after `FriendSystem`.
///
/// The server writes the author's name itself, from the session: a client cannot
/// write as someone else. It keeps no history apart from what waits in `MessageStore`.
class ChatSystem final : public ecs::System {
public:
    static constexpr std::size_t MaxMessagesInWindow = 5; // Per player. A choice, not a measure
    static constexpr float WindowSeconds = 5.0F;
    static constexpr float SecondsBetweenSaves = 1.0F;

    ChatSystem(const FriendStore& friends, MessageStore& messages) noexcept;

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    void deliverWaitingMessages(ecs::World& world);
    void handleMessages(ecs::World& world, float now);
    /// True if `name` may write now. It counts as a message.
    [[nodiscard]]
    bool withinRate(const std::string& name, float now);
    void saveWhenDue(float now);

    const FriendStore& _friends;
    MessageStore& _messages;
    std::set<std::string> _online; // Names, as of the last frame
    std::map<std::string, std::vector<float>> _lastMessages; // Times, within the window
    float _lastSave{0.0F};
};

} // namespace server
