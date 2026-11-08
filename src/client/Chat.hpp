#pragma once

#include "client/Components/Session.hpp"

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/// What a line typed in the chat means, and how a message is cut to fit its box.
/// Plain functions, with no window and no socket, so the tests can call them directly.
namespace client {

/// What to do with what was typed.
struct ChatOrder {
    struct Nothing { };
    struct ToGame {
        std::string text;
    };
    struct ToFriend {
        std::string name;
        std::string text;
    };
    struct Problem {
        std::string text; // Shown in the chat
    };
    std::variant<Nothing, ToGame, ToFriend, Problem> action;
};

/// Reads a line: to `target` if the right-click menu chose one, `/msg <friend> <text>`
/// for a friend, otherwise for the group.
///
/// A name can contain spaces, so `/msg` takes the longest name of a friend the text
/// starts with, followed by a space. Upper and lower case count the same in that search
/// (letters without accents only): the exact name is what goes to the server.
[[nodiscard]]
ChatOrder readChat(std::string_view input, const std::optional<std::string>& target,
    std::span<const FriendRow> friends);

/// Cuts `text` into lines for which `fits` is true: between words if it can, and inside
/// a word too long for a whole line, never inside a UTF-8 character.
[[nodiscard]]
std::vector<std::string> wrapChatText(
    std::string_view text, const std::function<bool(std::string_view)>& fits);

} // namespace client
