#include "server/FriendStore.hpp"

#include "server/AccountStore.hpp"
#include "server/TextFile.hpp"

#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace server {

namespace {

constexpr std::string_view Header = "# r-type friends: F name name, R from to (separated by tabs)";

} // namespace

FriendStore::FriendStore(std::filesystem::path file)
    : _file(std::move(file))
{
}

std::expected<FriendStore, std::string> FriendStore::load(const std::filesystem::path& file)
{
    FriendStore store(file);

    std::error_code error;
    if (!std::filesystem::exists(file, error)) {
        if (error || !store.save()) { // Tested now, not at the first request
            return std::unexpected("cannot create the friends file " + file.string());
        }
        return store;
    }

    std::ifstream input(file, std::ios::binary);
    if (!input) {
        return std::unexpected("cannot read the friends file " + file.string());
    }
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && !line.starts_with('#') && !store.readLine(line)) {
            ++store._damagedLines;
        }
    }
    if (store._damagedLines > 0 && !keepDamagedCopy(file)) {
        return std::unexpected("cannot save a copy of the damaged file " + file.string());
    }
    return store;
}

bool FriendStore::readLine(std::string_view line)
{
    if (line.size() < 2 || line[1] != '\t') {
        return false;
    }
    const char kind = line[0];
    line.remove_prefix(2);
    const std::size_t tab = line.find('\t');
    if (tab == std::string_view::npos) {
        return false;
    }
    const std::string first(line.substr(0, tab));
    const std::string second(line.substr(tab + 1));
    if (!AccountStore::isValidName(first) || !AccountStore::isValidName(second)
        || first == second) {
        return false;
    }

    if (kind == 'F') {
        if (isFull(first) || isFull(second) || friendsOf(first).contains(second)) {
            return false;
        }
        link(first, second);
        return true;
    }
    if (kind == 'R') {
        if (requestsFrom(first).size() >= MaxRequests || requestsTo(second).size() >= MaxRequests
            || !_sent[first].insert(second).second) {
            return false;
        }
        _received[second].insert(first);
        return true;
    }
    return false;
}

const FriendStore::Names& FriendStore::namesOf(const Links& links, const std::string& name)
{
    static const Names None;
    const auto found = links.find(name);
    return found != links.end() ? found->second : None;
}

bool FriendStore::isFull(const std::string& name) const
{
    return friendsOf(name).size() >= MaxFriends;
}

void FriendStore::link(const std::string& first, const std::string& second)
{
    _friends[first].insert(second);
    _friends[second].insert(first);
}

FriendStore::RequestResult FriendStore::request(const std::string& from, const std::string& to)
{
    if (from == to) {
        return RequestResult::Yourself;
    }
    if (friendsOf(from).contains(to) || requestsFrom(from).contains(to)) {
        return RequestResult::AlreadyLinked;
    }
    if (isFull(from) || isFull(to)) {
        return RequestResult::LimitReached;
    }
    if (requestsFrom(to).contains(from)) { // Both want it: nothing left to answer
        (void)dropRequest(to, from);
        link(from, to);
        _changed = true;
        return RequestResult::BecameFriends;
    }
    if (requestsFrom(from).size() >= MaxRequests || requestsTo(to).size() >= MaxRequests) {
        return RequestResult::LimitReached;
    }
    _sent[from].insert(to);
    _received[to].insert(from);
    _changed = true;
    return RequestResult::Sent;
}

FriendStore::AcceptResult FriendStore::accept(const std::string& from, const std::string& to)
{
    if (!requestsFrom(from).contains(to)) {
        return AcceptResult::NoRequest;
    }
    if (isFull(from) || isFull(to)) {
        return AcceptResult::LimitReached;
    }
    (void)dropRequest(from, to);
    link(from, to);
    _changed = true;
    return AcceptResult::Accepted;
}

bool FriendStore::dropRequest(const std::string& from, const std::string& to)
{
    const auto sent = _sent.find(from);
    if (sent == _sent.end() || sent->second.erase(to) == 0) {
        return false;
    }
    _received[to].erase(from);
    _changed = true;
    return true;
}

bool FriendStore::removeFriend(const std::string& name, const std::string& other)
{
    const auto found = _friends.find(name);
    if (found == _friends.end() || found->second.erase(other) == 0) {
        return false;
    }
    _friends[other].erase(name);
    _changed = true;
    return true;
}

const FriendStore::Names& FriendStore::friendsOf(const std::string& name) const
{
    return namesOf(_friends, name);
}

const FriendStore::Names& FriendStore::requestsTo(const std::string& name) const
{
    return namesOf(_received, name);
}

const FriendStore::Names& FriendStore::requestsFrom(const std::string& name) const
{
    return namesOf(_sent, name);
}

bool FriendStore::hasChanges() const noexcept
{
    return _changed;
}

bool FriendStore::save()
{
    std::ostringstream content;
    content << Header << '\n';
    for (const auto& [name, others] : _friends) {
        for (const std::string& other : others) {
            if (name < other) { // Each friendship is kept twice, and written once
                content << "F\t" << name << '\t' << other << '\n';
            }
        }
    }
    for (const auto& [from, targets] : _sent) {
        for (const std::string& to : targets) {
            content << "R\t" << from << '\t' << to << '\n';
        }
    }
    if (!replaceFile(_file, content.str())) {
        return false;
    }
    _changed = false;
    return true;
}

std::size_t FriendStore::damagedLines() const noexcept
{
    return _damagedLines;
}

} // namespace server
