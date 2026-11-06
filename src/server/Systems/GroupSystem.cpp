#include "server/Systems/GroupSystem.hpp"

#include "ecs/World/World.hpp"
#include "server/Components/ClientMessage.hpp"
#include "server/Components/NetClient.hpp"
#include "server/GameRoom.hpp"
#include "server/OnlinePlayers.hpp"
#include "server/Singleton.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

namespace server {

using namespace network::protocol;

namespace {

/// One group's state, as its members must see it.
GroupState stateOf(const Group& group)
{
    GroupState state;
    state.code = group.code;
    state.countdown = group.countdown
        ? static_cast<std::uint8_t>(std::max(1.0F, std::ceil(*group.countdown)))
        : std::uint8_t{0};
    for (const GroupMember& member : group.members) {
        if (member.name == group.leader) {
            state.leader = static_cast<std::uint8_t>(state.members.size());
        }
        state.members.push_back(
            GroupMemberState{.name = member.name, .ready = member.ready, .inGame = member.inGame});
    }
    return state;
}

void refuseStart(const OnlinePlayers& online, const std::string& name, Groups::StartProblem problem)
{
    online.tell(name,
        JoinResult{
            .code
            = problem == Groups::StartProblem::NotLeader ? JoinCode::NotLeader : JoinCode::NotReady,
            .playerId = NoPlayer,
        });
}

} // namespace

GroupSystem::GroupSystem(const FriendStore& friends)
    : _friends(friends)
{
}

void GroupSystem::update(ecs::World& world, const ecs::Time& time)
{
    Groups& groups = singleton<Groups>(world);
    updateOnline(world, groups);
    handleMessages(world, groups, time.elapsedSeconds());
    runCountdowns(world, groups, time);
    sendStates(world, groups);
}

void GroupSystem::updateOnline(ecs::World& world, Groups& groups)
{
    const OnlinePlayers online(world);
    for (const auto& [name, channel] : online.channels()) {
        if (!_online.contains(name)) {
            groups.add(name);
        }
    }
    for (const std::string& name : _online) {
        if (!online.contains(name)) {
            groups.remove(name);
            _lastSent.erase(name);
            _wrongCodes.erase(name);
        }
    }
    _online.clear();
    for (const auto& [name, channel] : online.channels()) {
        _online.insert(name);
    }

    // Whoever came back from a game, by any way, is free again
    for (Group& group : groups.all()) {
        for (GroupMember& member : group.members) {
            member.inGame = false;
        }
    }
    world.query<NetClient, InRoom>([&](ecs::Entity /*entity*/, NetClient& client, InRoom&) {
        if (Group* group = groups.of(client.name)) {
            for (GroupMember& member : group->members) {
                member.inGame = member.inGame || member.name == client.name;
            }
        }
    });
}

void GroupSystem::handleMessages(ecs::World& world, Groups& groups, float now)
{
    std::vector<ecs::Entity> messages;
    world.query<ClientMessage>([&](ecs::Entity entity, ClientMessage& message) {
        std::visit(
            [&]<typename T>(const T& /*payload*/) {
                if constexpr (std::is_same_v<T, JoinGame> || std::is_same_v<T, GroupCode>
                    || std::is_same_v<T, GroupJoin> || std::is_same_v<T, GroupLeave>
                    || std::is_same_v<T, GroupKick> || std::is_same_v<T, GroupPromote>
                    || std::is_same_v<T, GroupInvite> || std::is_same_v<T, GroupAnswer>
                    || std::is_same_v<T, GroupReady>) {
                    messages.push_back(entity);
                }
            },
            message.payload);
    });

    const OnlinePlayers online(world);
    for (const ecs::Entity entity : messages) {
        const ClientMessage message = world.getComponent<ClientMessage>(entity);
        world.destroyEntity(entity);
        const auto* client = world.tryGetComponent<NetClient>(message.client);
        if (client == nullptr || !online.contains(client->name)
            || world.hasComponent<InRoom>(message.client)) {
            continue; // Leaving during this frame, or playing: this is for the main page
        }
        const std::string name = client->name;
        const auto say = [&](std::optional<GroupOutcome> outcome) {
            if (outcome) {
                online.tell(name, GroupResult{.outcome = *outcome});
            }
        };

        if (std::holds_alternative<JoinGame>(message.payload)) {
            const auto problem = groups.startProblem(name);
            if (problem != Groups::StartProblem::None) {
                refuseStart(online, name, problem);
            } else if (Group* group = groups.of(name); !group->countdown) {
                group->countdown = groups.countdownSeconds;
            }
        } else if (const auto* code = std::get_if<GroupCode>(&message.payload)) {
            handleCode(groups, online, name, code->action);
        } else if (const auto* join = std::get_if<GroupJoin>(&message.payload)) {
            handleJoin(groups, online, name, join->code, now);
        } else if (std::holds_alternative<GroupLeave>(message.payload)) {
            say(groups.leave(name));
        } else if (const auto* kick = std::get_if<GroupKick>(&message.payload)) {
            const auto refused = groups.kick(name, kick->name);
            say(refused);
            if (!refused) {
                online.tell(kick->name, GroupResult{.outcome = GroupOutcome::RemovedFromGroup});
            }
        } else if (const auto* promote = std::get_if<GroupPromote>(&message.payload)) {
            say(groups.promote(name, promote->name));
        } else if (const auto* invite = std::get_if<GroupInvite>(&message.payload)) {
            handleInvite(groups, online, name, invite->name);
        } else if (const auto* answer = std::get_if<GroupAnswer>(&message.payload)) {
            if (answer->reply == FriendReply::Accept) {
                say(groups.accept(name, answer->name));
            } else {
                groups.refuse(name, answer->name);
            }
        } else if (const auto* ready = std::get_if<GroupReady>(&message.payload)) {
            groups.setReady(name, ready->ready);
        }
    }

    std::erase_if(_wrongCodes, [&](const auto& entry) {
        return !entry.second.empty() && now - entry.second.back() >= WrongCodeWindowSeconds;
    });
}

void GroupSystem::handleCode(
    Groups& groups, const OnlinePlayers& online, const std::string& name, GroupCodeAction action)
{
    Group* group = groups.of(name);
    if (group == nullptr || group->leader != name) {
        online.tell(name, GroupResult{.outcome = GroupOutcome::NotLeader});
        return;
    }
    group->code = action == GroupCodeAction::Generate ? newCode(groups) : std::string{};
    std::cout << "r-type_server: group of '" << name << "' now has "
              << (group->code.empty() ? "no code" : "the code " + group->code) << '\n';
}

void GroupSystem::handleJoin(Groups& groups, const OnlinePlayers& online, const std::string& name,
    const std::string& code, float now)
{
    auto& tries = _wrongCodes[name];
    std::erase_if(tries, [&](float time) { return now - time >= WrongCodeWindowSeconds; });
    if (tries.size() >= MaxWrongCodes) {
        online.tell(name, GroupResult{.outcome = GroupOutcome::TooManyTries});
        return;
    }
    const auto refused = groups.join(name, code);
    if (refused == GroupOutcome::UnknownCode) {
        tries.push_back(now);
    }
    if (refused) {
        online.tell(name, GroupResult{.outcome = *refused});
    }
}

void GroupSystem::handleInvite(Groups& groups, const OnlinePlayers& online, const std::string& name,
    const std::string& invited)
{
    if (!_friends.friendsOf(name).contains(invited) || !online.contains(invited)) {
        online.tell(name, GroupResult{.outcome = GroupOutcome::NotAvailable});
        return;
    }
    // Asking again tells them nothing new: nobody can flood a friend with invitations
    const Group* group = groups.of(name);
    const bool alreadyInvited = group != nullptr && group->invited.contains(invited);
    if (const auto refused = groups.invite(name, invited)) {
        online.tell(name, GroupResult{.outcome = *refused});
        return;
    }
    if (!alreadyInvited) {
        online.tell(invited, GroupInvited{.name = name});
    }
    online.tell(name, GroupResult{.outcome = GroupOutcome::Invited});
}

void GroupSystem::runCountdowns(ecs::World& world, Groups& groups, const ecs::Time& time)
{
    for (Group& group : groups.all()) {
        if (!group.countdown) {
            continue;
        }
        *group.countdown -= time.deltaSeconds();
        if (*group.countdown > 0.0F) {
            continue;
        }
        group.countdown.reset();
        // Sessions in the order of the group: colours stay the same in the game
        std::vector<ecs::Entity> players;
        for (GroupMember& member : group.members) {
            world.query<NetClient>([&](ecs::Entity entity, NetClient& client) {
                if (client.name == member.name && world.hasComponent<LoggedIn>(entity)
                    && !world.hasComponent<Disconnecting>(entity)) {
                    players.push_back(entity);
                }
            });
            member.ready = false;
            member.inGame = true;
        }
        startGame(world, players, time.elapsedSeconds());
    }
}

void GroupSystem::sendStates(ecs::World& world, Groups& groups)
{
    const OnlinePlayers online(world);
    for (const Group& group : groups.all()) {
        const GroupState state = stateOf(group);
        for (const GroupMember& member : group.members) {
            const auto last = _lastSent.find(member.name);
            if (last == _lastSent.end() || last->second != state) {
                online.tell(member.name, state);
                _lastSent[member.name] = state;
            }
        }
    }
}

std::string GroupSystem::newCode(const Groups& groups)
{
    std::uniform_int_distribution<std::size_t> sign(0, GroupCodeAlphabet.size() - 1);
    while (true) {
        std::string code;
        for (std::size_t i = 0; i < GroupCodeSize; ++i) {
            code += GroupCodeAlphabet.at(sign(_random));
        }
        if (!groups.codeInUse(code)) {
            return code;
        }
    }
}

} // namespace server
