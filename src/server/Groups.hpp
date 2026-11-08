#pragma once

#include "network/Protocol/Protocol.hpp"

#include <cstdint>
#include <list>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace server {

struct GroupMember {
    std::string name;
    bool ready{false};
    bool inGame{false};
};

/// Up to 4 players who talk and play together. Never empty; the leader is one of them.
struct Group {
    std::string code; // Empty: only an invitation lets someone in
    std::string leader;
    std::vector<GroupMember> members;
    std::set<std::string> invited;
    std::set<std::string> kicked; // Their code no longer works here; an invitation still does
    std::optional<float> countdown; // Seconds before the game starts

    [[nodiscard]]
    bool inGame() const noexcept;
    [[nodiscard]]
    bool hasMember(const std::string& name) const noexcept;
};

/// Every group of the server, as plain data with no network: each online player is in
/// exactly one, alone at first. A singleton component: `GroupSystem` changes it, the
/// chat reads it. Groups live in memory only, and are gone with their last member.
///
/// Who may do what is checked here. An order that is refused changes nothing.
class Groups {
public:
    using Outcome = network::protocol::GroupOutcome;

    /// Seconds between the leader's Play and the game. A value and not a constant, so
    /// that tests about something else do not have to wait.
    float countdownSeconds{3.0F};

    /// `name` came online: a new group of their own.
    void add(const std::string& name);
    /// `name` went offline. The next member becomes leader if they led.
    void remove(const std::string& name);

    [[nodiscard]]
    Group* of(const std::string& name) noexcept;
    [[nodiscard]]
    const std::list<Group>& all() const noexcept;
    [[nodiscard]]
    std::list<Group>& all() noexcept;
    [[nodiscard]]
    bool codeInUse(const std::string& code) const noexcept;

    /// Nothing if it worked. A new member is not ready, and stops any countdown.
    std::optional<Outcome> join(const std::string& name, const std::string& code);
    std::optional<Outcome> leave(const std::string& name);
    std::optional<Outcome> kick(const std::string& leader, const std::string& name);
    std::optional<Outcome> promote(const std::string& leader, const std::string& name);
    /// Also lets a kicked player back in.
    std::optional<Outcome> invite(const std::string& leader, const std::string& name);
    std::optional<Outcome> accept(const std::string& name, const std::string& from);
    void refuse(const std::string& name, const std::string& from);
    /// The leader has nothing to confirm: their Play says it.
    void setReady(const std::string& name, bool ready);

    /// What stops the leader of `group` from starting. Nothing: they can.
    enum class StartProblem : std::uint8_t { None, NotLeader, NotReady };
    [[nodiscard]]
    StartProblem startProblem(const std::string& name) noexcept;

private:
    /// Out of their group, which goes if it is left empty.
    void detach(const std::string& name);
    /// `name`, out of any group, into `group`.
    void enter(Group& group, const std::string& name);

    std::list<Group> _groups; // A list: joining a group must not move the others
};

} // namespace server
