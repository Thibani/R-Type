#include "server/MessageStore.hpp"

#include "network/Protocol/PacketCodec.hpp"
#include "server/AccountStore.hpp"
#include "server/TextFile.hpp"

#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>
#include <utility>

namespace server {

namespace {

constexpr std::string_view Header = "# r-type private messages: to from text (separated by tabs)";

} // namespace

MessageStore::MessageStore(std::filesystem::path file)
    : _file(std::move(file))
{
}

std::expected<MessageStore, std::string> MessageStore::load(const std::filesystem::path& file)
{
    MessageStore store(file);

    std::error_code error;
    if (!std::filesystem::exists(file, error)) {
        if (error || !store.save()) { // Tested now, not at the first message
            return std::unexpected("cannot create the messages file " + file.string());
        }
        return store;
    }

    std::ifstream input(file, std::ios::binary);
    if (!input) {
        return std::unexpected("cannot read the messages file " + file.string());
    }
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && !line.starts_with('#') && !store.readLine(line)) {
            ++store._damagedLines;
        }
    }
    store._changed = false;
    if (store._damagedLines > 0 && !keepDamagedCopy(file)) {
        return std::unexpected("cannot save a copy of the damaged file " + file.string());
    }
    return store;
}

bool MessageStore::readLine(std::string_view line)
{
    const std::size_t first = line.find('\t');
    if (first == std::string_view::npos) {
        return false;
    }
    const std::size_t second = line.find('\t', first + 1);
    if (second == std::string_view::npos) {
        return false;
    }
    const std::string to(line.substr(0, first));
    std::string from(line.substr(first + 1, second - first - 1));
    std::string text(line.substr(second + 1));
    if (!AccountStore::isValidName(to) || !AccountStore::isValidName(from)
        || !network::protocol::isValidChatText(text)) {
        return false;
    }
    return add(to, StoredMessage{.from = std::move(from), .text = std::move(text)});
}

bool MessageStore::add(const std::string& to, StoredMessage message)
{
    auto& waiting = _waiting[to];
    if (waiting.size() >= MaxMessages) {
        return false;
    }
    waiting.push_back(std::move(message));
    _changed = true;
    return true;
}

std::vector<StoredMessage> MessageStore::take(const std::string& to)
{
    const auto found = _waiting.find(to);
    if (found == _waiting.end()) {
        return {};
    }
    std::vector<StoredMessage> messages(std::make_move_iterator(found->second.begin()),
        std::make_move_iterator(found->second.end()));
    _waiting.erase(found);
    _changed = true;
    return messages;
}

bool MessageStore::hasChanges() const noexcept
{
    return _changed;
}

bool MessageStore::save()
{
    std::ostringstream content;
    content << Header << '\n';
    for (const auto& [to, messages] : _waiting) {
        for (const StoredMessage& message : messages) {
            content << to << '\t' << message.from << '\t' << message.text << '\n';
        }
    }
    if (!replaceFile(_file, content.str())) {
        return false;
    }
    _changed = false;
    return true;
}

std::size_t MessageStore::damagedLines() const noexcept
{
    return _damagedLines;
}

} // namespace server
