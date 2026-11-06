#include "server/Systems/FriendSystem.hpp"

#include "ecs/World/World.hpp"
#include "network/Protocol/Packets.hpp"
#include "server/Components/ClientMessage.hpp"
#include "server/Components/NetClient.hpp"
#include "server/OnlinePlayers.hpp"

#include <iostream>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace server {

using namespace network::protocol;

namespace {

/// Tells `name` that `other` is `state`. Nothing if `name` is offline: they get their
/// whole list when they come back.
void entry(const OnlinePlayers& online, const std::string& name, const std::string& other,
    FriendState state)
{
    online.tell(name, FriendEntry{.name = other, .state = state});
}

FriendState stateOf(const OnlinePlayers& online, const std::string& name)
{
    return online.contains(name) ? FriendState::Online : FriendState::Offline;
}

/// Tells both that they are now friends, each with the other's status.
void tellFriends(const OnlinePlayers& online, const std::string& first, const std::string& second)
{
    entry(online, first, second, stateOf(online, second));
    entry(online, second, first, stateOf(online, first));
}

/// Tells both that they no longer have anything to do with each other.
void tellRemoved(const OnlinePlayers& online, const std::string& first, const std::string& second)
{
    entry(online, first, second, FriendState::Removed);
    entry(online, second, first, FriendState::Removed);
}

FriendCode codeOf(FriendStore::RequestResult result) noexcept
{
    switch (result) {
    case FriendStore::RequestResult::Sent:
    case FriendStore::RequestResult::BecameFriends:
        return FriendCode::Ok;
    case FriendStore::RequestResult::AlreadyLinked:
        return FriendCode::AlreadyLinked;
    case FriendStore::RequestResult::Yourself:
        return FriendCode::Yourself;
    case FriendStore::RequestResult::LimitReached:
        return FriendCode::LimitReached;
    }
    return FriendCode::LimitReached;
}

void handle(const OnlinePlayers& online, const AccountStore& accounts, FriendStore& friends,
    const std::string& name, const FriendRequest& request)
{
    if (accounts.find(request.name) == nullptr) {
        online.tell(name, FriendResult{.code = FriendCode::UnknownName});
        return;
    }
    const auto result = friends.request(name, request.name);
    online.tell(name, FriendResult{.code = codeOf(result)});
    if (result == FriendStore::RequestResult::Sent) {
        entry(online, name, request.name, FriendState::RequestSent);
        entry(online, request.name, name, FriendState::RequestReceived);
    } else if (result == FriendStore::RequestResult::BecameFriends) {
        tellFriends(online, name, request.name);
    }
}

void handle(const OnlinePlayers& online, const AccountStore& /*accounts*/, FriendStore& friends,
    const std::string& name, const FriendAnswer& answer)
{
    if (answer.reply == FriendReply::Refuse) {
        if (friends.dropRequest(answer.name, name)) {
            tellRemoved(online, name, answer.name);
        }
        return;
    }
    const auto result = friends.accept(answer.name, name);
    if (result == FriendStore::AcceptResult::Accepted) {
        tellFriends(online, name, answer.name);
    } else if (result == FriendStore::AcceptResult::LimitReached) {
        online.tell(name, FriendResult{.code = FriendCode::LimitReached});
    }
}

void handle(const OnlinePlayers& online, const AccountStore& /*accounts*/, FriendStore& friends,
    const std::string& name, const FriendRemove& remove)
{
    if (friends.removeFriend(name, remove.name) || friends.dropRequest(name, remove.name)) {
        tellRemoved(online, name, remove.name);
    }
}

} // namespace

FriendSystem::FriendSystem(const AccountStore& accounts, FriendStore& friends) noexcept
    : _accounts(accounts)
    , _friends(friends)
{
}

void FriendSystem::update(ecs::World& world, const ecs::Time& time)
{
    updateOnline(world); // First: a newcomer gets their list before any change to it
    handleMessages(world);
    saveWhenDue(time);
}

void FriendSystem::updateOnline(ecs::World& world)
{
    const OnlinePlayers online(world);

    for (const auto& [name, channel] : online.channels()) {
        if (_online.contains(name)) {
            continue;
        }
        for (const std::string& other : _friends.friendsOf(name)) {
            entry(online, name, other, stateOf(online, other));
            entry(online, other, name, FriendState::Online);
        }
        for (const std::string& other : _friends.requestsTo(name)) {
            entry(online, name, other, FriendState::RequestReceived);
        }
        for (const std::string& other : _friends.requestsFrom(name)) {
            entry(online, name, other, FriendState::RequestSent);
        }
    }
    for (const std::string& name : _online) {
        if (online.contains(name)) {
            continue;
        }
        for (const std::string& other : _friends.friendsOf(name)) {
            entry(online, other, name, FriendState::Offline);
        }
    }

    _online.clear();
    for (const auto& [name, channel] : online.channels()) {
        _online.insert(name);
    }
}

void FriendSystem::handleMessages(ecs::World& world)
{
    std::vector<ecs::Entity> messages;
    world.query<ClientMessage>([&](ecs::Entity entity, ClientMessage& message) {
        if (std::holds_alternative<FriendRequest>(message.payload)
            || std::holds_alternative<FriendAnswer>(message.payload)
            || std::holds_alternative<FriendRemove>(message.payload)) {
            messages.push_back(entity);
        }
    });

    const OnlinePlayers online(world);
    for (const ecs::Entity entity : messages) {
        const ClientMessage message = world.getComponent<ClientMessage>(entity);
        world.destroyEntity(entity);
        const auto* client = world.tryGetComponent<NetClient>(message.client);
        if (client == nullptr || !online.contains(client->name)) {
            continue; // Leaving during this frame
        }
        const std::string name = client->name;
        std::visit(
            [&]<typename T>(const T& payload) {
                if constexpr (std::is_same_v<T, FriendRequest> || std::is_same_v<T, FriendAnswer>
                    || std::is_same_v<T, FriendRemove>) {
                    handle(online, _accounts, _friends, name, payload);
                }
            },
            message.payload);
    }
}

void FriendSystem::saveWhenDue(const ecs::Time& time)
{
    if (!_friends.hasChanges() || time.elapsedSeconds() - _lastSave < SecondsBetweenSaves) {
        return;
    }
    _lastSave = time.elapsedSeconds();
    if (!_friends.save()) { // Tried again in a second: the changes stay in memory
        std::cerr << "r-type_server: cannot save the friends file\n";
    }
}

} // namespace server
