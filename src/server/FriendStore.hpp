#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>

namespace server {

/// Who is friends with whom, and the requests waiting for an answer, in memory
/// and in a text file with one line per link:
///
///     F <tab> <name> <tab> <name>     two friends
///     R <tab> <from> <tab> <to>       a request waiting for an answer
///
/// A name has no control character (`AccountStore::isValidName`), so a tab can
/// separate names that contain spaces.
///
/// It knows nothing about the ECS or the network. Only the game thread uses it.
/// Changes only stay in memory until `save()`: `FriendSystem` saves at most once
/// per second, so that a client cannot make the game wait for the disk.
class FriendStore {
public:
    static constexpr std::size_t MaxFriends = 50; // Per player
    static constexpr std::size_t MaxRequests = 50; // Sent, and received, per player

    enum class RequestResult : std::uint8_t {
        Sent,
        BecameFriends, // The other had already asked
        AlreadyLinked, // Already friends, or already asked
        Yourself,
        LimitReached,
    };

    enum class AcceptResult : std::uint8_t {
        Accepted,
        NoRequest,
        LimitReached,
    };

    /// Reads `file`, or creates it if it does not exist yet. Fails if it cannot
    /// be read or written: the server must not start and lose friends later.
    /// A damaged line is skipped, and the file is first copied to `<file>.damaged`.
    [[nodiscard]]
    static std::expected<FriendStore, std::string> load(const std::filesystem::path& file);

    /// `from` asks `to`. The caller checks that `to` is an account.
    [[nodiscard]]
    RequestResult request(const std::string& from, const std::string& to);

    /// `to` accepts the request of `from`.
    [[nodiscard]]
    AcceptResult accept(const std::string& from, const std::string& to);

    /// Forgets the request of `from` to `to`, refused or cancelled. False if none.
    [[nodiscard]]
    bool dropRequest(const std::string& from, const std::string& to);

    /// `name` and `other` are no longer friends. False if they were not.
    [[nodiscard]]
    bool removeFriend(const std::string& name, const std::string& other);

    using Names = std::set<std::string, std::less<>>;

    [[nodiscard]]
    const Names& friendsOf(const std::string& name) const;
    [[nodiscard]]
    const Names& requestsTo(const std::string& name) const;
    [[nodiscard]]
    const Names& requestsFrom(const std::string& name) const;

    /// True if something changed since the last successful `save()`.
    [[nodiscard]]
    bool hasChanges() const noexcept;

    /// Writes the file, through a temporary one. False if it failed.
    [[nodiscard]]
    bool save();

    /// Lines of the file that could not be read when it was loaded.
    [[nodiscard]]
    std::size_t damagedLines() const noexcept;

private:
    explicit FriendStore(std::filesystem::path file);

    using Links = std::map<std::string, Names, std::less<>>;

    /// Adds one line of the file. False if it breaks a rule of the store.
    bool readLine(std::string_view line);
    [[nodiscard]]
    static const Names& namesOf(const Links& links, const std::string& name);
    [[nodiscard]]
    bool isFull(const std::string& name) const;
    void link(const std::string& first, const std::string& second);

    std::filesystem::path _file;
    Links _friends; // Both ways: each friendship is there twice
    Links _received; // to -> from
    Links _sent; // from -> to
    bool _changed{false};
    std::size_t _damagedLines{0};
};

} // namespace server
