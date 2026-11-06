#include "client/Chat.hpp"

#include "network/Protocol/PacketCodec.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace client {

using network::protocol::FriendState;
using network::protocol::isValidChatText;

namespace {

char lowered(char c) noexcept
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool startsWithAnyCase(std::string_view text, std::string_view start) noexcept
{
    return text.size() >= start.size()
        && std::ranges::equal(text.substr(0, start.size()), start, {}, lowered, lowered);
}

std::string_view trimmed(std::string_view text) noexcept
{
    while (text.starts_with(' ')) {
        text.remove_prefix(1);
    }
    while (text.ends_with(' ')) {
        text.remove_suffix(1);
    }
    return text;
}

/// A message ready to go, or why it cannot.
ChatOrder checked(ChatOrder order, std::string_view text)
{
    if (!isValidChatText(text)) {
        return {.action = ChatOrder::Problem{.text = "Message too long: 200 characters at most."}};
    }
    return order;
}

ChatOrder readMsg(std::string_view rest, std::span<const FriendRow> friends)
{
    const FriendRow* best = nullptr;
    for (const FriendRow& person : friends) {
        const bool isFriend
            = person.state == FriendState::Online || person.state == FriendState::Offline;
        const std::string withSpace = person.name + ' ';
        if (isFriend && (best == nullptr || person.name.size() > best->name.size())
            && startsWithAnyCase(rest, withSpace)) {
            best = &person;
        }
    }
    if (best == nullptr) {
        // The name alone, with nothing to say, is not the same mistake
        const bool nameOnly = std::ranges::any_of(friends, [&](const FriendRow& person) {
            return rest.size() == person.name.size() && startsWithAnyCase(rest, person.name);
        });
        return {.action = ChatOrder::Problem{
                    .text = nameOnly ? "Usage: /msg name message" : "Not in your friends."}};
    }
    const std::string_view text = trimmed(rest.substr(best->name.size()));
    if (text.empty()) {
        return {.action = ChatOrder::Problem{.text = "Usage: /msg name message"}};
    }
    return checked(
        {.action = ChatOrder::ToFriend{.name = best->name, .text = std::string(text)}}, text);
}

} // namespace

ChatOrder readChat(std::string_view input, const std::optional<std::string>& target,
    std::span<const FriendRow> friends, bool inGame)
{
    const std::string_view text = trimmed(input);
    if (text.empty()) {
        return {.action = ChatOrder::Nothing{}};
    }
    if (target) {
        return checked(
            {.action = ChatOrder::ToFriend{.name = *target, .text = std::string(text)}}, text);
    }
    constexpr std::string_view Command = "/msg";
    if (startsWithAnyCase(text, Command)
        && (text.size() == Command.size() || text[Command.size()] == ' ')) {
        return readMsg(trimmed(text.substr(Command.size())), friends);
    }
    if (!inGame) {
        return {.action = ChatOrder::Problem{.text = "Not in a group yet."}};
    }
    return checked({.action = ChatOrder::ToGame{.text = std::string(text)}}, text);
}

std::vector<std::string> wrapChatText(
    std::string_view text, const std::function<bool(std::string_view)>& fits)
{
    const auto isContinuation
        = [](char c) { return (static_cast<unsigned char>(c) & 0xC0U) == 0x80U; };

    std::vector<std::string> lines;
    std::string current;
    while (!text.empty()) {
        const std::size_t space = text.find(' ');
        std::string_view word = text.substr(0, space);
        text.remove_prefix(space == std::string_view::npos ? text.size() : space + 1);

        std::string longer
            = current.empty() ? std::string(word) : current + ' ' + std::string(word);
        if (fits(longer)) {
            current = std::move(longer);
            continue;
        }
        if (!current.empty()) {
            lines.push_back(std::exchange(current, {}));
        }
        // A word too long for a line: as many whole characters as fit, again and again
        while (!word.empty() && !fits(word)) {
            std::size_t size = word.size();
            while (size > 1 && !fits(word.substr(0, size))) {
                --size;
                while (size > 1 && isContinuation(word[size])) {
                    --size;
                }
            }
            lines.emplace_back(word.substr(0, size));
            word.remove_prefix(size);
        }
        current = std::string(word);
    }
    if (!current.empty()) {
        lines.push_back(std::move(current));
    }
    return lines;
}

} // namespace client
