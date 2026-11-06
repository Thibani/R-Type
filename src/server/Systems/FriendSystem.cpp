#include "server/Systems/FriendSystem.hpp"

#include "ecs/World/World.hpp"
#include "network/Protocol/Packets.hpp"
#include "server/Components/ClientMessage.hpp"
#include "server/Components/NetClient.hpp"

#include <iostream>
#include <map>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace server {

using namespace network::protocol;

namespace {

/// The channels of the players online, by name.
class Online {
public:
    explicit Online(ecs::World& world)
    {
        world.query<NetClient, LoggedIn>(
            [&](ecs::Entity entity, NetClient& client, LoggedIn& /*loggedIn*/) {
                if (!world.hasComponent<Disconnecting>(entity)) {
                    _channels.emplace(client.name, &client.channel);
                }
            });
    }

    [[nodiscard]]
    bool contains(const std::string& name) const
    {
        return _channels.contains(name);
    }

    /// Nothing if `name` is offline: they get their whole list when they come back.
    void tell(const std::string& name, const Payload& payload) const
    {
        if (const auto found = _channels.find(name); found != _channels.end()) {
            found->second->queueReliable(payload);
        }
    }

    /// Tells `name` that `other` is `state`.
    void entry(const std::string& name, const std::string& other, FriendState state) const
    {
        tell(name, FriendEntry{.name = other, .state = state});
    }

    /// Tells both that they are now friends, each with the other's status.
    void friends(const std::string& first, const std::string& second) const
    {
        entry(first, second, stateOf(second));
        entry(second, first, stateOf(first));
    }

    /// Tells both that they no longer have anything to do with each other.
    void removed(const std::string& first, const std::string& second) const
    {
        entry(first, second, FriendState::Removed);
        entry(second, first, FriendState::Removed);
    }

    [[nodiscard]]
    FriendState stateOf(const std::string& name) const
    {
        return contains(name) ? FriendState::Online : FriendState::Offline;
    }

    [[nodiscard]]
    const std::map<std::string, network::ReliableChannel*>& channels() const noexcept
    {
        return _channels;
    }

private:
    std::map<std::string, network::ReliableChannel*> _channels;
};

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

void handle(const Online& online, const AccountStore& accounts, FriendStore& friends,
    const std::string& name, const FriendRequest& request)
{
    if (accounts.find(request.name) == nullptr) {
        online.tell(name, FriendResult{.code = FriendCode::UnknownName});
        return;
    }
    const auto result = friends.request(name, request.name);
    online.tell(name, FriendResult{.code = codeOf(result)});
    if (result == FriendStore::RequestResult::Sent) {
        online.entry(name, request.name, FriendState::RequestSent);
        online.entry(request.name, name, FriendState::RequestReceived);
    } else if (result == FriendStore::RequestResult::BecameFriends) {
        online.friends(name, request.name);
    }
}

void handle(const Online& online, const AccountStore& /*accounts*/, FriendStore& friends,
    const std::string& name, const FriendAnswer& answer)
{
    if (answer.reply == FriendReply::Refuse) {
        if (friends.dropRequest(answer.name, name)) {
            online.removed(name, answer.name);
        }
        return;
    }
    const auto result = friends.accept(answer.name, name);
    if (result == FriendStore::AcceptResult::Accepted) {
        online.friends(name, answer.name);
    } else if (result == FriendStore::AcceptResult::LimitReached) {
        online.tell(name, FriendResult{.code = FriendCode::LimitReached});
    }
}

void handle(const Online& online, const AccountStore& /*accounts*/, FriendStore& friends,
    const std::string& name, const FriendRemove& remove)
{
    if (friends.removeFriend(name, remove.name) || friends.dropRequest(name, remove.name)) {
        online.removed(name, remove.name);
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
    const Online online(world);

    for (const auto& [name, channel] : online.channels()) {
        if (_online.contains(name)) {
            continue;
        }
        for (const std::string& other : _friends.friendsOf(name)) {
            online.entry(name, other, online.stateOf(other));
            online.entry(other, name, FriendState::Online);
        }
        for (const std::string& other : _friends.requestsTo(name)) {
            online.entry(name, other, FriendState::RequestReceived);
        }
        for (const std::string& other : _friends.requestsFrom(name)) {
            online.entry(name, other, FriendState::RequestSent);
        }
    }
    for (const std::string& name : _online) {
        if (online.contains(name)) {
            continue;
        }
        for (const std::string& other : _friends.friendsOf(name)) {
            online.entry(other, name, FriendState::Offline);
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

    const Online online(world);
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
