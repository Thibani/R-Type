#include "server/Groups.hpp"

#include <algorithm>
#include <utility>

namespace server {

using Outcome = Groups::Outcome;

bool Group::inGame() const noexcept
{
    return std::ranges::any_of(members, &GroupMember::inGame);
}

bool Group::hasMember(const std::string& name) const noexcept
{
    return std::ranges::find(members, name, &GroupMember::name) != members.end();
}

void Groups::add(const std::string& name)
{
    Group group;
    group.leader = name;
    group.members.push_back(GroupMember{.name = name});
    _groups.push_back(std::move(group));
}

void Groups::remove(const std::string& name)
{
    detach(name);
    for (Group& group : _groups) {
        group.invited.erase(name);
    }
}

Group* Groups::of(const std::string& name) noexcept
{
    const auto found
        = std::ranges::find_if(_groups, [&](const Group& group) { return group.hasMember(name); });
    return found != _groups.end() ? &*found : nullptr;
}

const std::list<Group>& Groups::all() const noexcept
{
    return _groups;
}

std::list<Group>& Groups::all() noexcept
{
    return _groups;
}

bool Groups::codeInUse(const std::string& code) const noexcept
{
    return std::ranges::find(_groups, code, &Group::code) != _groups.end();
}

void Groups::detach(const std::string& name)
{
    Group* group = of(name);
    if (group == nullptr) {
        return;
    }
    std::erase_if(group->members, [&](const GroupMember& member) { return member.name == name; });
    group->countdown.reset();
    if (group->members.empty()) {
        _groups.remove_if([&](const Group& other) { return &other == group; });
    } else if (group->leader == name) {
        group->leader = group->members.front().name;
    }
}

void Groups::enter(Group& group, const std::string& name)
{
    detach(name);
    group.members.push_back(GroupMember{.name = name});
    group.invited.erase(name);
    group.countdown.reset();
}

std::optional<Outcome> Groups::join(const std::string& name, const std::string& code)
{
    const auto found = std::ranges::find(_groups, code, &Group::code);
    if (code.empty() || found == _groups.end()) {
        return Outcome::UnknownCode;
    }
    Group& group = *found;
    if (group.hasMember(name)) {
        return std::nullopt; // Already there: nothing to do, nothing to say
    }
    if (group.kicked.contains(name)) {
        return Outcome::Kicked;
    }
    if (group.inGame()) {
        return Outcome::GroupInGame;
    }
    if (group.members.size() >= network::protocol::MaxPlayers) {
        return Outcome::GroupFull;
    }
    enter(group, name);
    return std::nullopt;
}

std::optional<Outcome> Groups::leave(const std::string& name)
{
    const Group* group = of(name);
    if (group == nullptr || group->members.size() < 2) {
        return Outcome::Alone;
    }
    detach(name);
    add(name);
    return std::nullopt;
}

std::optional<Outcome> Groups::kick(const std::string& leader, const std::string& name)
{
    Group* group = of(leader);
    if (group == nullptr || group->leader != leader) {
        return Outcome::NotLeader;
    }
    if (name == leader || !group->hasMember(name)) {
        return Outcome::NotMember;
    }
    if (std::ranges::any_of(group->members,
            [&](const GroupMember& member) { return member.name == name && member.inGame; })) {
        return Outcome::GroupInGame;
    }
    group->kicked.insert(name);
    detach(name);
    add(name);
    return std::nullopt;
}

std::optional<Outcome> Groups::promote(const std::string& leader, const std::string& name)
{
    Group* group = of(leader);
    if (group == nullptr || group->leader != leader) {
        return Outcome::NotLeader;
    }
    if (!group->hasMember(name)) {
        return Outcome::NotMember;
    }
    group->leader = name;
    group->countdown.reset();
    return std::nullopt;
}

std::optional<Outcome> Groups::invite(const std::string& leader, const std::string& name)
{
    Group* group = of(leader);
    if (group == nullptr || group->leader != leader) {
        return Outcome::NotLeader;
    }
    if (group->inGame()) {
        return Outcome::GroupInGame;
    }
    if (group->members.size() >= network::protocol::MaxPlayers) {
        return Outcome::GroupFull;
    }
    if (group->hasMember(name)) {
        return Outcome::NotAvailable;
    }
    group->invited.insert(name);
    group->kicked.erase(name);
    return std::nullopt;
}

std::optional<Outcome> Groups::accept(const std::string& name, const std::string& from)
{
    Group* group = of(from);
    if (group == nullptr || !group->invited.contains(name)) {
        return Outcome::InvitationGone;
    }
    if (group->inGame()) {
        return Outcome::GroupInGame;
    }
    if (group->members.size() >= network::protocol::MaxPlayers) {
        return Outcome::GroupFull;
    }
    enter(*group, name);
    return std::nullopt;
}

void Groups::refuse(const std::string& name, const std::string& from)
{
    if (Group* group = of(from)) {
        group->invited.erase(name);
    }
}

void Groups::setReady(const std::string& name, bool ready)
{
    Group* group = of(name);
    if (group == nullptr) {
        return;
    }
    for (GroupMember& member : group->members) {
        if (member.name == name && member.ready != ready) {
            member.ready = ready;
            group->countdown.reset();
        }
    }
}

Groups::StartProblem Groups::startProblem(const std::string& name) noexcept
{
    const Group* group = of(name);
    if (group == nullptr || group->leader != name) {
        return StartProblem::NotLeader;
    }
    const bool allReady = std::ranges::all_of(group->members, [&](const GroupMember& member) {
        return !member.inGame && (member.ready || member.name == name);
    });
    return allReady ? StartProblem::None : StartProblem::NotReady;
}

} // namespace server
