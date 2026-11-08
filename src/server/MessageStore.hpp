#pragma once

#include <cstddef>
#include <deque>
#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace server {

/// A private message waiting for its reader.
struct StoredMessage {
    std::string from;
    std::string text;
};

/// The private messages of players who are offline, in memory and in a text file
/// with one message per line, oldest first. A name or a text has no control
/// character, so a tab can separate them:
///
///     <to> <tab> <from> <tab> <text>
///
/// The file is not encrypted: whoever runs the server can read it.
/// It knows nothing about the ECS or the network. Only the game thread uses it.
/// Changes only stay in memory until `save()`, as in `FriendStore`.
class MessageStore {
public:
    static constexpr std::size_t MaxMessages = 100; // Waiting, per player

    /// Reads `file`, or creates it if it does not exist yet. Fails if it cannot
    /// be read or written. A damaged line is skipped, and the file is first
    /// copied to `<file>.damaged`.
    [[nodiscard]]
    static std::expected<MessageStore, std::string> load(const std::filesystem::path& file);

    /// Keeps a message for `to`. False if they already have `MaxMessages` waiting.
    [[nodiscard]]
    bool add(const std::string& to, StoredMessage message);

    /// Removes and returns the messages waiting for `to`, oldest first.
    [[nodiscard]]
    std::vector<StoredMessage> take(const std::string& to);

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
    explicit MessageStore(std::filesystem::path file);

    bool readLine(std::string_view line);

    std::filesystem::path _file;
    std::map<std::string, std::deque<StoredMessage>, std::less<>> _waiting;
    bool _changed{false};
    std::size_t _damagedLines{0};
};

} // namespace server
