#include "server/Systems/ChatSystem.hpp"

#include "ecs/World/World.hpp"
#include "network/Protocol/Packets.hpp"
#include "server/Components/ClientMessage.hpp"
#include "server/Components/NetClient.hpp"
#include "server/OnlinePlayers.hpp"

#include <iostream>
#include <utility>
#include <variant>

namespace server {

using namespace network::protocol;

ChatSystem::ChatSystem(const FriendStore& friends, MessageStore& messages) noexcept
    : _friends(friends)
    , _messages(messages)
{
}

void ChatSystem::update(ecs::World& world, const ecs::Time& time)
{
    deliverWaitingMessages(world);
    handleMessages(world, time.elapsedSeconds());
    saveWhenDue(time.elapsedSeconds());
}

void ChatSystem::deliverWaitingMessages(ecs::World& world)
{
    const OnlinePlayers online(world);
    std::set<std::string> names;
    for (const auto& [name, channel] : online.channels()) {
        names.insert(name);
        if (_online.contains(name)) {
            continue;
        }
        for (StoredMessage& message : _messages.take(name)) {
            channel->queueReliable(ChatMessage{.kind = ChatKind::Received,
                .name = std::move(message.from),
                .text = std::move(message.text)});
        }
    }
    _online = std::move(names);
}

bool ChatSystem::withinRate(const std::string& name, float now)
{
    auto& times = _lastMessages[name];
    std::erase_if(times, [&](float time) { return now - time >= WindowSeconds; });
    if (times.size() >= MaxMessagesInWindow) {
        return false;
    }
    times.push_back(now);
    return true;
}

void ChatSystem::handleMessages(ecs::World& world, float now)
{
    std::vector<ecs::Entity> received;
    world.query<ClientMessage>([&](ecs::Entity entity, ClientMessage& message) {
        if (std::holds_alternative<ChatSend>(message.payload)
            || std::holds_alternative<PrivateSend>(message.payload)) {
            received.push_back(entity);
        }
    });

    const OnlinePlayers online(world);
    for (const ecs::Entity entity : received) {
        const ClientMessage message = world.getComponent<ClientMessage>(entity);
        world.destroyEntity(entity);
        const auto* client = world.tryGetComponent<NetClient>(message.client);
        if (client == nullptr || !online.contains(client->name)) {
            continue; // Leaving during this frame
        }
        const std::string& name = client->name;

        if (const auto* chat = std::get_if<ChatSend>(&message.payload)) {
            if (!world.hasComponent<InGame>(message.client)) {
                continue; // Nobody to talk to; the client says so itself
            }
            if (!withinRate(name, now)) {
                online.tell(name, ChatRefused{.reason = ChatRefusal::TooFast});
                continue;
            }
            const ChatMessage line{.kind = ChatKind::Game, .name = name, .text = chat->text};
            world.query<NetClient, InGame>([&](ecs::Entity /*entity*/, NetClient& player, InGame&) {
                player.channel.queueReliable(line);
            });
            continue;
        }

        const auto& letter = std::get<PrivateSend>(message.payload);
        if (!_friends.friendsOf(name).contains(letter.name)) {
            online.tell(name, ChatRefused{.reason = ChatRefusal::NotFriend});
            continue;
        }
        if (!withinRate(name, now)) {
            online.tell(name, ChatRefused{.reason = ChatRefusal::TooFast});
            continue;
        }
        if (online.contains(letter.name)) {
            online.tell(letter.name,
                ChatMessage{.kind = ChatKind::Received, .name = name, .text = letter.text});
        } else if (!_messages.add(letter.name, StoredMessage{.from = name, .text = letter.text})) {
            online.tell(name, ChatRefused{.reason = ChatRefusal::MailboxFull});
            continue;
        }
        online.tell(
            name, ChatMessage{.kind = ChatKind::Sent, .name = letter.name, .text = letter.text});
    }

    // Who has not written for a whole window needs no memory
    std::erase_if(_lastMessages, [&](const auto& entry) {
        return !entry.second.empty() && now - entry.second.back() >= WindowSeconds;
    });
}

void ChatSystem::saveWhenDue(float now)
{
    if (!_messages.hasChanges() || now - _lastSave < SecondsBetweenSaves) {
        return;
    }
    _lastSave = now;
    if (!_messages.save()) { // Tried again in a second: the messages stay in memory
        std::cerr << "r-type_server: cannot save the messages file\n";
    }
}

} // namespace server
