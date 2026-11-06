#include "client/Systems/InterfaceSystem.hpp"

#include "client/Assets.hpp"
#include "client/Chat.hpp"
#include "client/Components/NetEntity.hpp"
#include "client/Components/Session.hpp"
#include "client/Components/Settings.hpp"
#include "client/Gamepad.hpp"
#include "client/Settings.hpp"
#include "client/Singleton.hpp"
#include "client/Ui/Layout.hpp"
#include "client/Ui/Theme.hpp"
#include "client/Ui/View.hpp"
#include "client/Ui/Widgets.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"

#include <SFML/Window/Keyboard.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace client {

namespace {

using widgets::box;
using widgets::centredText;
using widgets::text;
using widgets::textWidth;

/// "12450" → "12 450", as in the mockups.
std::string formatScore(std::uint32_t score)
{
    std::string digits = std::to_string(score);
    for (std::ptrdiff_t i = std::ssize(digits) - 3; i > 0; i -= 3) {
        digits.insert(static_cast<std::size_t>(i), " ");
    }
    return digits;
}

std::string playerLabel(std::size_t player)
{
    return "P" + std::to_string(player + 1);
}

/// A big title in the display font, centred on the screen.
void centredTitle(
    sf::RenderTarget& target, const Assets& assets, std::string_view label, unsigned size, float y)
{
    text(target, assets.display, label, size, theme::Text,
        {(theme::UiWidth - textWidth(assets.display, label, size)) / 2.0F, y});
}

/// Small game name and gold lines in the corners of a full screen.
void screenDecoration(sf::RenderTarget& target, const Assets& assets)
{
    widgets::goldDetail(target, {48.0F, 40.0F}, 110.0F);
    text(target, assets.display, "R-TYPE", 30, theme::Text, {48.0F, 61.0F});
    widgets::goldDetail(target, {1122.0F, 660.0F}, 110.0F);
}

void centredHelp(sf::RenderTarget& target, const Assets& assets, std::string_view help, float y)
{
    text(target, assets.body, help, 15, theme::Muted,
        {(theme::UiWidth - textWidth(assets.body, help, 15)) / 2.0F, y});
}

void errorBox(
    sf::RenderTarget& target, const Assets& assets, sf::FloatRect rect, std::string_view message)
{
    box(target, rect, theme::ErrorBackground, theme::Red, 1.0F);
    text(target, assets.bodyBold, message, 16, theme::Red,
        rect.position + sf::Vector2f{14.0F, 9.0F});
}

/// The banner of one player: tag, pattern and ship if present, "Waiting..." if not.
/// Everything follows the banner's width, 224 on the normal screens.
/// Names use the body font: the display font has few accents.
void crewBanner(sf::RenderTarget& target, const Assets& assets, ColourMode mode, sf::FloatRect rect,
    std::size_t player, const PlayerSlot& slot, bool isYou)
{
    const float scale = rect.size.x / 224.0F;
    const auto at = [&](float x, float y) { return rect.position + (sf::Vector2f{x, y} * scale); };
    const auto sized = [&](float w, float h) { return sf::Vector2f{w, h} * scale; };
    const auto font
        = [&](unsigned size) { return static_cast<unsigned>(static_cast<float>(size) * scale); };

    const auto id = static_cast<std::uint8_t>(player);
    const sf::Color colour = theme::Palettes.at(static_cast<std::size_t>(mode)).players.at(player);
    widgets::banner(target, rect, theme::Surface, slot.present ? colour : theme::Hull);
    text(target, assets.bodyBold, playerLabel(player), font(26),
        slot.present ? theme::Text : theme::Muted, at(24.0F, 22.0F));
    const sf::FloatRect nameArea{at(20.0F, 231.0F), sized(184.0F, 48.0F)};
    if (!slot.present) {
        centredText(target, assets.body, "+", font(32), theme::Muted,
            {at(32.0F, 108.0F), sized(160.0F, 80.0F)});
        centredText(target, assets.body, "Waiting...", font(22), theme::Text, nameArea);
        return;
    }
    if (isYou) {
        text(target, assets.bodyBold, "YOU", 14, theme::Blue, at(82.0F, 29.0F));
    }
    widgets::pattern(target, assets, mode, {at(162.0F, 26.0F), sized(36.0F, 24.0F)}, id);
    widgets::ship(target, assets, mode, {at(32.0F, 108.0F), sized(160.0F, 80.0F)}, id);
    centredText(target, assets.bodyBold, slot.name,
        widgets::fittedSize(assets.bodyBold, slot.name, font(26), nameArea.size.x), theme::Text,
        nameArea);
    box(target, {at(88.0F, 300.0F), sized(48.0F, 3.0F)}, colour, colour, 0.0F);
}

/// One star per character of the password (a UTF-8 character can use several bytes).
std::string hidden(std::string_view password)
{
    const auto isCharacterStart
        = [](char c) { return (static_cast<unsigned char>(c) & 0xC0U) != 0x80U; };
    return std::string(
        static_cast<std::size_t>(std::ranges::count_if(password, isCharacterStart)), '*');
}

/// The connection's state, or why it failed, on one line of the form.
void connectMessage(sf::RenderTarget& target, const Assets& assets, const Session& session)
{
    const bool failed = !session.error.empty();
    const std::string& message = failed ? session.error : session.status;
    // The longest messages only fit in a smaller size
    const unsigned size
        = textWidth(assets.body, message, 16) <= layout::ConnectStatusWidth ? 16 : 14;
    text(target, assets.body, message, size, failed ? theme::Red : theme::Blue,
        layout::ConnectStatus);
}

void drawConnect(sf::RenderTarget& target, const Assets& assets, ColourMode mode,
    const Session& session, bool compact)
{
    if (!compact) { // Then the view only shows the panel, with the hint inside it
        widgets::goldDetail(target, {64.0F, 64.0F}, 136.0F);
        text(target, assets.display, "R-TYPE", 152, theme::Text, {60.0F, 101.0F});
        box(target, {{64.0F, 269.0F}, {590.0F, 1.0F}}, theme::Hull, theme::Hull, 0.0F);
        widgets::ship(target, assets, mode, {{170.0F, 340.0F}, {320.0F, 160.0F}}, 0);
    }

    widgets::panel(target, layout::ConnectPanel, theme::Surface, theme::Hull, 1.5F, 18.0F);
    widgets::goldDetail(target, {716.0F, 134.0F}, 62.0F);
    text(target, assets.display, "Join a game", 44, theme::Text, {716.0F, 152.0F});
    widgets::field(target, assets, layout::NameInput, "Player name", session.name,
        session.focus == layout::NameField, "16 characters max");
    widgets::field(target, assets, layout::PasswordInput, "Password", hidden(session.password),
        session.focus == layout::PasswordField, "8 to 64 characters");
    widgets::field(target, assets, layout::AddressInput, "Server address", session.address,
        session.focus == layout::AddressField);
    widgets::field(target, assets, layout::PortInput, "Port", session.port,
        session.focus == layout::PortField);
    connectMessage(target, assets, session);
    widgets::button(
        target, assets, layout::Login, "Log in", true, session.focus == layout::LoginButton);
    widgets::button(target, assets, layout::CreateAccount, "Create account", false,
        session.focus == layout::CreateAccountButton);
    widgets::button(target, assets, layout::OpenSettings, "Settings", false,
        session.focus == layout::SettingsButton);
    widgets::button(
        target, assets, layout::Quit, "Quit", false, session.focus == layout::QuitButton);

    text(target, assets.body, "Tab: next field  ·  Enter: log in  ·  Esc: quit", 15, theme::Muted,
        compact ? sf::Vector2f{716.0F, 545.0F} : sf::Vector2f{64.0F, 682.0F});
}

std::string chatLabel(const ChatLine& line)
{
    switch (line.kind) {
    case ChatLine::Kind::Game:
        return line.name + ": " + line.text;
    case ChatLine::Kind::Received:
        return "[From " + line.name + "] " + line.text;
    case ChatLine::Kind::Sent:
        return "[To " + line.name + "] " + line.text;
    case ChatLine::Kind::Notice:
        break;
    }
    return line.text;
}

sf::Color chatColour(ChatLine::Kind kind) noexcept
{
    switch (kind) {
    case ChatLine::Kind::Game:
        return theme::Text;
    case ChatLine::Kind::Received:
    case ChatLine::Kind::Sent:
        return theme::Gold;
    case ChatLine::Kind::Notice:
        break;
    }
    return theme::Muted;
}

/// The chat: the newest messages at the bottom, a pale frame, and the typing line
/// only while typing. Private messages are told apart by their words, not only their colour.
void drawChat(sf::RenderTarget& target, const Assets& assets, const Session& session,
    sf::FloatRect rect, bool focused)
{
    box(target, rect, session.chatOpen ? theme::ChatFrameOpen : theme::ChatFrame,
        focused || session.chatOpen ? theme::Blue : theme::ChatBorder, focused ? 2.0F : 1.0F);

    constexpr float Margin = 8.0F;
    const float width = rect.size.x - (2.0F * Margin);
    const auto fits = [&](std::string_view part) {
        return textWidth(assets.body, part, layout::ChatTextSize) <= width;
    };
    float bottom = rect.position.y + rect.size.y - Margin;
    if (session.chatOpen) {
        const std::string prefix
            = session.chatTarget ? "To [" + *session.chatTarget + "]: " : std::string{};
        const float prefixWidth = textWidth(assets.bodyBold, prefix, layout::ChatTextSize);
        const sf::FloatRect line{{rect.position.x, bottom - layout::ChatInputHeight + Margin},
            {rect.size.x, layout::ChatInputHeight}};
        box(target, line, theme::InputBackground, theme::Hull, 1.0F);
        const sf::Vector2f inside = line.position + sf::Vector2f{Margin, 6.0F};
        text(target, assets.bodyBold, prefix, layout::ChatTextSize, theme::Gold, inside);
        // The end of what is typed, like a field whose text is too long
        std::string_view shown = session.chatInput;
        const auto isContinuation
            = [](char c) { return (static_cast<unsigned char>(c) & 0xC0U) == 0x80U; };
        while (!shown.empty()
            && textWidth(assets.body, shown, layout::ChatTextSize) > width - prefixWidth - 6.0F) {
            shown.remove_prefix(1);
            while (!shown.empty() && isContinuation(shown.front())) {
                shown.remove_prefix(1);
            }
        }
        const sf::Vector2f typed = inside + sf::Vector2f{prefixWidth, 0.0F};
        text(target, assets.body, shown, layout::ChatTextSize, theme::Text, typed);
        box(target,
            {{typed.x + textWidth(assets.body, shown, layout::ChatTextSize) + 2.0F,
                 line.position.y + 6.0F},
                {2.0F, layout::ChatInputHeight - 12.0F}},
            theme::Blue, theme::Blue, 0.0F);
        bottom = line.position.y - 4.0F;
    }

    const float top = rect.position.y + Margin;
    std::size_t next
        = session.chat.size() > session.chatScroll ? session.chat.size() - session.chatScroll : 0;
    while (next > 0 && bottom - layout::ChatLineHeight >= top) {
        const ChatLine& line = session.chat.at(--next);
        const auto parts = wrapChatText(chatLabel(line), fits);
        for (auto part = parts.rbegin();
            part != parts.rend() && bottom - layout::ChatLineHeight >= top; ++part) {
            bottom -= layout::ChatLineHeight;
            text(target, assets.body, *part, layout::ChatTextSize, chatColour(line.kind),
                {rect.position.x + Margin, bottom});
        }
    }
}

/// A small button of the friends panel.
void rowButton(sf::RenderTarget& target, const Assets& assets, sf::FloatRect rect,
    std::string_view label, bool focused)
{
    box(target, rect, theme::Surface, focused ? theme::Blue : theme::Hull, focused ? 2.0F : 1.0F);
    centredText(target, assets.bodyBold, label, 13, theme::Text, rect);
}

/// One member of the group: role, ship, name, state, and for the leader what can be done.
/// Their colour is the one they will have in the game, which gives them the same order.
void memberCard(sf::RenderTarget& target, const Assets& assets, ColourMode mode,
    const Session& session, std::size_t place, int interfaceSize)
{
    const auto& group = session.group;
    const auto& member = group.members.at(place);
    const bool compact = layout::isCompact(interfaceSize);
    const sf::FloatRect card = layout::memberCard(interfaceSize, place);
    const bool isLeader = place == group.leader;
    if (compact) { // Too short for the point of a banner
        widgets::panel(
            target, card, theme::Surface, isLeader ? theme::Gold : theme::Hull, 1.5F, 8.0F);
    } else {
        widgets::banner(target, card, theme::Surface, isLeader ? theme::Gold : theme::Hull);
    }
    const auto at = [&](float x, float y) { return card.position + sf::Vector2f{x, y}; };
    const std::string role
        = std::string(isLeader ? "Leader" : "") + (member.name == session.name ? " (you)" : "");
    centredText(target, assets.bodyBold, role, 13, isLeader ? theme::Gold : theme::Blue,
        {at(0.0F, compact ? 6.0F : 16.0F), {card.size.x, 16.0F}});
    widgets::ship(target, assets, mode,
        compact ? sf::FloatRect{at(18.0F, 26.0F), {70.0F, 35.0F}}
                : sf::FloatRect{at(16.0F, 44.0F), {128.0F, 64.0F}},
        static_cast<std::uint8_t>(place));
    const sf::FloatRect nameArea{at(6.0F, compact ? 64.0F : 120.0F), {card.size.x - 12.0F, 26.0F}};
    centredText(target, assets.bodyBold, member.name,
        widgets::fittedSize(assets.bodyBold, member.name, 20, nameArea.size.x), theme::Text,
        nameArea);
    const std::string_view state = member.inGame ? "In game"
        : isLeader                               ? ""
        : member.ready                           ? "Ready"
                                                 : "Not ready";
    centredText(target, assets.body, state, 14,
        member.ready || member.inGame ? theme::Green : theme::Muted,
        {at(0.0F, compact ? 90.0F : 152.0F), {card.size.x, 18.0F}});

    if (leadsTheGroup(session) && !isLeader) {
        const int number = static_cast<int>(place);
        rowButton(target, assets, layout::kickMember(interfaceSize, place), "Kick",
            session.focus == layout::FirstKickButton + number);
        rowButton(target, assets, layout::promoteMember(interfaceSize, place), "Lead",
            session.focus == layout::FirstPromoteButton + number);
    }
}

/// What the big button says: it starts the game for the leader, and says "ready" for the others.
std::string playLabel(const Session& session)
{
    if (session.group.countdown > 0) {
        return "Starting in " + std::to_string(session.group.countdown);
    }
    if (leadsTheGroup(session)) {
        return "Play";
    }
    const auto me = std::ranges::find(
        session.group.members, session.name, &network::protocol::GroupMemberState::name);
    return me != session.group.members.end() && me->ready ? "Not ready" : "Ready";
}

/// The code of the group, who may use it, and how to join another group.
void drawGroupCode(
    sf::RenderTarget& target, const Assets& assets, const Session& session, int interfaceSize)
{
    const auto& code = session.group.code;
    const std::string label = code.empty() ? "No code" : "Code: " + code;
    const sf::Vector2f where = layout::groupCodeLabel(interfaceSize);
    const float room = layout::newGroupCode(interfaceSize).position.x - where.x - 8.0F;
    text(target, assets.bodyBold, label, widgets::fittedSize(assets.bodyBold, label, 22, room),
        code.empty() ? theme::Muted : theme::Gold, where);
    if (leadsTheGroup(session)) {
        rowButton(target, assets, layout::newGroupCode(interfaceSize),
            code.empty() ? "Get a code" : "New code", session.focus == layout::NewGroupCodeButton);
        if (!code.empty()) {
            rowButton(target, assets, layout::deleteGroupCode(interfaceSize), "Delete",
                session.focus == layout::DeleteGroupCodeButton);
        }
    }
    widgets::field(target, assets, layout::groupCodeInput(interfaceSize), "Join with a code",
        session.groupCodeInput, session.focus == layout::GroupCodeField);
    rowButton(target, assets, layout::groupJoin(interfaceSize), "Join",
        session.focus == layout::GroupJoinButton);
}

/// The main page is the lobby: the group, the way into the game, and the chat. A place
/// in the game only exists once in it, so no "P1" here.
void drawHome(sf::RenderTarget& target, const Assets& assets, ColourMode mode,
    const Session& session, int interfaceSize)
{
    const bool compact = layout::isCompact(interfaceSize);
    if (!compact) {
        screenDecoration(target, assets);
    }
    drawGroupCode(target, assets, session, interfaceSize);
    if (session.group.members.empty()) { // Alone, until the first GROUP_STATE comes
        const sf::FloatRect card = layout::memberCard(interfaceSize, 0);
        centredText(target, assets.bodyBold, session.name, 20, theme::Text,
            {card.position, {card.size.x, 40.0F}});
    }
    for (std::size_t place = 0; place < session.group.members.size(); ++place) {
        memberCard(target, assets, mode, session, place, interfaceSize);
    }

    const sf::FloatRect message = layout::homeMessage(interfaceSize);
    if (!session.invitations.empty()) {
        box(target, message, theme::Surface, theme::Gold, 1.0F);
        const std::string invitation = session.invitations.front() + " invites you to their group";
        constexpr float Room = 150.0F; // The two buttons
        text(target, assets.bodyBold, invitation,
            widgets::fittedSize(assets.bodyBold, invitation, 16, message.size.x - 24.0F - Room),
            theme::Text, message.position + sf::Vector2f{12.0F, 12.0F});
        rowButton(target, assets, layout::acceptInvitation(interfaceSize), "Accept",
            session.focus == layout::AcceptInvitationButton);
        rowButton(target, assets, layout::refuseInvitation(interfaceSize), "Refuse",
            session.focus == layout::RefuseInvitationButton);
    } else if (!session.error.empty()) {
        errorBox(target, assets, message, session.error);
    }

    widgets::button(target, assets, layout::play(interfaceSize), playLabel(session), true,
        session.focus == layout::PlayButton);
    if (session.group.members.size() >= 2) {
        widgets::button(target, assets, layout::leaveGroup(interfaceSize), "Leave group", false,
            session.focus == layout::LeaveGroupButton);
    }
    widgets::button(target, assets, layout::homeSettings(interfaceSize), "Settings", false,
        session.focus == layout::HomeSettingsButton);
    widgets::button(target, assets, layout::logOut(interfaceSize), "Log out", false,
        session.focus == layout::LogOutButton);
    centredHelp(target, assets, "Tab: next  ·  Enter: select", compact ? 538.0F : 690.0F);
    drawChat(target, assets, session, layout::homeChat(interfaceSize),
        !session.settingsOpen && !session.chatOpen && session.focus == layout::ChatBox);
}

std::string_view friendStatus(network::protocol::FriendState state) noexcept
{
    using network::protocol::FriendState;
    switch (state) {
    case FriendState::Online:
        return "Online";
    case FriendState::RequestReceived:
        return "Wants to be your friend";
    case FriendState::RequestSent:
        return "Request sent";
    case FriendState::Offline:
    case FriendState::Removed:
        break;
    }
    return "Offline";
}

/// Closed: the name of the bar, friends online and requests waiting, in words and numbers.
void drawFriendsClosed(
    sf::RenderTarget& target, const Assets& assets, const Session& session, sf::FloatRect bar)
{
    using network::protocol::FriendState;
    const auto count = [&](FriendState state) {
        return std::ranges::count(session.friends, state, &FriendRow::state);
    };
    box(target, bar, theme::Surface, theme::Hull, 1.0F);
    centredText(target, assets.body, "Friends", 13, theme::Text,
        {bar.position + sf::Vector2f{0.0F, 16.0F}, {bar.size.x, 18.0F}});
    centredText(target, assets.bodyBold, std::to_string(count(FriendState::Online)), 22,
        theme::Green, {bar.position + sf::Vector2f{0.0F, 40.0F}, {bar.size.x, 28.0F}});
    centredText(target, assets.body, "online", 12, theme::Muted,
        {bar.position + sf::Vector2f{0.0F, 68.0F}, {bar.size.x, 16.0F}});
    if (const auto requests = count(FriendState::RequestReceived); requests > 0) {
        const sf::FloatRect badge{
            bar.position + sf::Vector2f{8.0F, 96.0F}, {bar.size.x - 16.0F, 26.0F}};
        box(target, badge, theme::ErrorBackground, theme::Red, 1.0F);
        centredText(
            target, assets.bodyBold, std::to_string(requests) + " new", 12, theme::Red, badge);
    }
}

/// Open: adding a friend, then one row per person.
void drawFriendsOpen(
    sf::RenderTarget& target, const Assets& assets, const Session& session, sf::FloatRect bar)
{
    using network::protocol::FriendState;
    box(target, bar, theme::Surface, theme::Hull, 1.0F);
    text(target, assets.display, "Friends", 26, theme::Text,
        bar.position + sf::Vector2f{layout::FriendsPadding, 10.0F});
    widgets::field(target, assets, layout::friendNameInput(bar), "Exact name", session.friendName,
        session.focus == layout::FriendNameField);
    rowButton(
        target, assets, layout::friendAdd(bar), "Add", session.focus == layout::FriendAddButton);
    text(target, assets.body, session.friendMessage, 13, theme::Blue,
        bar.position + sf::Vector2f{layout::FriendsPadding, 128.0F});

    const auto buttons = layout::friendButtons(session, bar);
    const auto last
        = std::min(session.friends.size(), session.friendScroll + layout::friendRowsShown(bar));
    for (std::size_t row = session.friendScroll; row < last; ++row) {
        const FriendRow& person = session.friends.at(row);
        const sf::FloatRect line = layout::friendRow(bar, row - session.friendScroll);
        const bool asked = person.state == FriendState::RequestReceived;
        const float room = bar.size.x - (2.0F * layout::FriendsPadding) - (asked ? 136.0F : 68.0F);
        text(target, assets.bodyBold, person.name,
            widgets::fittedSize(assets.bodyBold, person.name, 16, room), theme::Text,
            line.position + sf::Vector2f{layout::FriendsPadding, 5.0F});
        text(target, assets.body, asked ? "Asked you" : friendStatus(person.state), 13,
            person.state == FriendState::Online ? theme::Green : theme::Muted,
            line.position + sf::Vector2f{layout::FriendsPadding, 27.0F});
    }
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        const layout::FriendButton& button = buttons.at(i);
        const bool focused = session.focus == layout::FirstFriendButton + static_cast<int>(i);
        std::string_view label = "Remove";
        if (button.action == layout::FriendAction::Accept) {
            label = "Accept";
        } else if (button.action == layout::FriendAction::Refuse) {
            label = "Refuse";
        } else if (session.friends.at(button.row).state == FriendState::RequestSent) {
            label = "Cancel";
        }
        rowButton(target, assets, button.rect, label, focused);
    }
    if (const auto menu = layout::friendMenuButton(session, bar)) {
        box(target, *menu, theme::Surface, theme::Blue, 2.0F);
        centredText(target, assets.bodyBold, "Message", 14, theme::Text, *menu);
    }
    if (const auto invite = layout::friendInviteButton(session, bar)) {
        box(target, *invite, theme::Surface, theme::Blue, 2.0F);
        centredText(target, assets.bodyBold, "Invite", 14, theme::Text, *invite);
    }
    if (session.friends.size() > layout::friendRowsShown(bar)) {
        text(target, assets.body, "PgUp / PgDn or wheel: more", 13, theme::Muted,
            bar.position + sf::Vector2f{layout::FriendsPadding, bar.size.y - 24.0F});
    }
}

/// Up to 4 banners, centred.
void drawBanners(sf::RenderTarget& target, const Assets& assets, ColourMode mode,
    const Session& session, std::span<const std::size_t> players, float y, int interfaceSize)
{
    const sf::Vector2f size = layout::bannerSize(interfaceSize);
    const float gap = layout::bannerGap(interfaceSize);
    const float step = size.x + gap;
    const float total = (static_cast<float>(players.size()) * step) - gap;
    for (std::size_t n = 0; n < players.size(); ++n) {
        const float x = ((theme::UiWidth - total) / 2.0F) + (static_cast<float>(n) * step);
        const std::size_t player = players[n];
        crewBanner(target, assets, mode, {{x, y}, size}, player, session.players.at(player),
            session.playerId == player);
    }
}

void drawWaitingRoom(sf::RenderTarget& target, const Assets& assets, ColourMode mode,
    const Session& session, int interfaceSize)
{
    const bool compact = layout::isCompact(interfaceSize);
    if (!compact) {
        screenDecoration(target, assets);
    }
    centredTitle(
        target, assets, "Waiting for players", compact ? 44U : 64U, compact ? 126.0F : 76.0F);
    constexpr std::array<std::size_t, 4> everyone{0, 1, 2, 3};
    drawBanners(target, assets, mode, session, everyone, compact ? 182.0F : 204.0F, interfaceSize);
    widgets::button(
        target, assets, layout::leave(interfaceSize), "Leave", false, session.focus == 0);
    centredHelp(target, assets,
        "Server " + session.address + ":" + session.port + "  ·  Esc: leave",
        compact ? 538.0F : 690.0F);
}

/// Health of each player's ship, from the snapshots (0 if no ship).
std::array<std::uint8_t, 4> shipHealth(ecs::World& world)
{
    std::array<std::uint8_t, 4> health{};
    world.query<NetEntity>([&](ecs::Entity /*entity*/, NetEntity& data) {
        if (data.kind == network::protocol::EntityKind::PlayerShip && data.owner < health.size()) {
            health.at(data.owner) = data.health;
        }
    });
    return health;
}

/// "Arrows: move  ·  Space: fire  ·  Esc: menu", with the player's own keys.
std::string controlsReminder(const Settings& settings)
{
    const auto name = [&](Action action) {
        return std::string(
            keyName(settings.keys.at(static_cast<std::size_t>(action))).value_or("?"));
    };
    constexpr std::size_t moves = 4; // Up, Down, Left, Right come first in Action
    const bool arrows = std::ranges::equal(
        std::span(settings.keys).first<moves>(), std::span(Settings::DefaultKeys).first<moves>());
    const std::string move = arrows ? "Arrows: move"
                                    : "Move: " + name(Action::Up) + " " + name(Action::Left) + " "
            + name(Action::Down) + " " + name(Action::Right);
    std::string keyboard = move + "  ·  " + name(Action::Fire) + ": fire  ·  Esc: menu";
    const auto fire = settings.buttons.at(static_cast<std::size_t>(Action::Fire));
    if (!gamepad::anyConnected()) {
        return keyboard;
    }
    return keyboard + "  ·  Pad: stick"
        + (fire ? ", " + std::to_string(*fire) + ": fire" : std::string()) + ", Start: menu";
}

/// One player in the game: colour bar, tag, pattern, name, then health or state.
void playerCard(sf::RenderTarget& target, const Assets& assets, ColourMode mode, sf::FloatRect card,
    std::size_t player, const PlayerSlot& slot, bool isYou, std::uint8_t points)
{
    const sf::Vector2f origin = card.position;
    const sf::Color colour = theme::Palettes.at(static_cast<std::size_t>(mode)).players.at(player);
    widgets::panel(target, card, theme::Surface, theme::Hull, 1.5F, 10.0F);
    box(target, {origin + sf::Vector2f{0.0F, 14.0F}, {3.0F, card.size.y - 28.0F}}, colour, colour,
        0.0F);
    text(target, assets.bodyBold, playerLabel(player), 15, theme::Text,
        origin + sf::Vector2f{14.0F, 9.0F});
    if (isYou) {
        text(target, assets.bodyBold, "YOU", 14, theme::Blue, origin + sf::Vector2f{46.0F, 10.0F});
    }
    if (slot.present && !slot.down) {
        widgets::pattern(target, assets, mode,
            {origin + sf::Vector2f{card.size.x - 49.0F, 9.0F}, {36.0F, 20.0F}},
            static_cast<std::uint8_t>(player));
    }
    constexpr unsigned NameSize = 20;
    const float nameWidth = card.size.x - 28.0F;
    text(target, assets.bodyBold, slot.name,
        widgets::fittedSize(assets.bodyBold, slot.name, NameSize, nameWidth), theme::Text,
        origin + sf::Vector2f{14.0F, 31.0F});

    const sf::Vector2f state = origin + sf::Vector2f{14.0F, 62.0F};
    if (slot.left) {
        text(target, assets.body, "Disconnected", 14, theme::Muted, state);
    } else if (slot.down) {
        text(target, assets.bodyBold, "Down", 14, theme::Red, state);
    } else {
        text(target, assets.bodyBold, "HP", 14, theme::Text, state);
        for (std::uint8_t pip = 0; pip < 3; ++pip) {
            const sf::Color fill = pip >= points ? theme::InputBackground
                : points == 1                    ? theme::Red
                                                 : theme::Green;
            box(target,
                {state + sf::Vector2f{28.0F + (static_cast<float>(pip) * 26.0F), 3.0F},
                    {22.0F, 10.0F}},
                fill, theme::Hull, 1.0F);
        }
    }
}

void drawHud(sf::RenderTarget& target, const Assets& assets, ecs::World& world,
    const Session& session, float now, sf::FloatRect area)
{
    constexpr float margin = 20.0F;
    const float left = area.position.x + margin;
    const float rightEdge = area.position.x + area.size.x - margin;
    const float firstRow = area.position.y + margin;

    // Team score, top left
    const std::string score = formatScore(session.score);
    const float scoreWidth = std::max(textWidth(assets.bodyBold, "TEAM SCORE", 13),
                                 textWidth(assets.bodyBold, score, 32))
        + 36.0F;
    widgets::panel(
        target, {{left, firstRow}, {scoreWidth, 64.0F}}, theme::Surface, theme::Hull, 1.5F, 10.0F);
    text(target, assets.bodyBold, "TEAM SCORE", 13, theme::Muted, {left + 18.0F, firstRow + 9.0F});
    text(target, assets.bodyBold, score, 32, theme::Text, {left + 18.0F, firstRow + 28.0F});

    // One card per player, right-aligned. On a new row when they would reach
    // the score, or the left of the screen
    const auto health = shipHealth(world);
    const Settings& settings = singleton<Settings>(world);
    float right = rightEdge;
    float row = firstRow;
    for (std::size_t back = session.players.size(); back > 0; --back) {
        const std::size_t i = back - 1;
        const PlayerSlot& slot = session.players.at(i);
        if (!slot.present && !slot.left) {
            continue;
        }
        const sf::Vector2f size = layout::PlayerCard;
        if (right - size.x < (row == firstRow ? left + scoreWidth + margin : left)) {
            right = rightEdge;
            row += size.y + margin;
        }
        playerCard(target, assets, settings.colourMode, {{right - size.x, row}, size}, i, slot,
            session.playerId == i, health.at(i));
        right -= size.x + 10.0F;
    }

    // Under the score, away from the cards: they can change without hiding it
    if (!session.notice.empty() && now < session.noticeUntil) {
        const float width = textWidth(assets.bodyBold, session.notice, 16) + 28.0F;
        const sf::FloatRect toast{{left, firstRow + 84.0F}, {width, 40.0F}};
        widgets::panel(target, toast, theme::Surface, theme::Blue, 1.5F, 8.0F);
        text(target, assets.bodyBold, session.notice, 16, theme::Text,
            toast.position + sf::Vector2f{14.0F, 12.0F});
    }

    const std::string reminder = controlsReminder(settings);
    const float reminderWidth = textWidth(assets.body, reminder, 15) + 24.0F;
    const float bottom = area.position.y + area.size.y - margin;
    box(target, {{left, bottom - 32.0F}, {reminderWidth, 32.0F}}, theme::Surface, theme::Hull,
        1.0F);
    text(target, assets.body, reminder, 15, theme::Muted, {left + 12.0F, bottom - 23.0F});
}

void drawMenu(
    sf::RenderTarget& target, const Assets& assets, const Session& session, sf::FloatRect area)
{
    box(target, area, theme::Overlay, theme::Overlay, 0.0F);
    widgets::panel(target, layout::MenuPanel, theme::Surface, theme::Hull, 1.5F, 18.0F);
    widgets::goldDetail(target, {460.0F, 183.0F}, 72.0F);
    centredTitle(target, assets, "Menu", 64, 205.0F);
    const std::string_view note = "The game keeps running for the other players.";
    text(target, assets.body, note, 15, theme::Muted,
        {(theme::UiWidth - textWidth(assets.body, note, 15)) / 2.0F, 272.0F});
    widgets::button(
        target, assets, layout::Resume, "Resume", true, session.focus == layout::ResumeButton);
    widgets::button(target, assets, layout::MenuSettings, "Settings", false,
        session.focus == layout::MenuSettingsButton);
    widgets::button(target, assets, layout::LeaveGame, "Leave game", false,
        session.focus == layout::LeaveButton);
}

/// The background of a settings row: a cyan outline if focused.
void rowFocus(sf::RenderTarget& target, sf::FloatRect rect, bool focused)
{
    if (focused) {
        box(target, rect, sf::Color::Transparent, theme::Blue, 2.0F);
    }
}

void volumeRow(
    sf::RenderTarget& target, const Assets& assets, const Settings& settings, bool focused)
{
    const sf::FloatRect rect = layout::settingsRow(layout::VolumeRow);
    rowFocus(target, rect, focused);
    const sf::Vector2f inside = rect.position + sf::Vector2f{16.0F, 14.0F};
    text(target, assets.body, "Sound effects", 20, theme::Text, inside);
    const std::string value
        = settings.volume == 0 ? "Muted" : std::to_string(settings.volume) + " %";
    text(target, assets.body, value, 20, theme::Blue,
        {rect.position.x + rect.size.x - 16.0F - textWidth(assets.body, value, 20), inside.y});

    const sf::FloatRect track = layout::VolumeTrack;
    const float level = track.size.x * static_cast<float>(settings.volume)
        / static_cast<float>(Settings::MaxVolume);
    box(target, track, theme::Hull, theme::Hull, 0.0F);
    box(target, {track.position, {level, track.size.y}}, theme::Blue, theme::Blue, 0.0F);
    box(target, {track.position + sf::Vector2f{level - 5.0F, -9.0F}, {10.0F, 22.0F}}, theme::Blue,
        theme::Blue, 0.0F);
}

/// A row with a "< value >" selector. A click left of its centre goes back.
void selectorRow(sf::RenderTarget& target, const Assets& assets, int row, std::string_view label,
    sf::FloatRect selector, std::string_view value, bool focused)
{
    const sf::FloatRect rect = layout::settingsRow(row);
    rowFocus(target, rect, focused);
    text(target, assets.body, label, 20, theme::Text, rect.position + sf::Vector2f{16.0F, 14.0F});
    box(target, selector, theme::InputBackground, theme::Hull, 1.0F);
    centredText(target, assets.body, value, 19, theme::Text, selector);
    constexpr float ArrowWidth = 28.0F;
    centredText(target, assets.bodyBold, "<", 20, theme::Blue,
        {selector.position, {ArrowWidth, selector.size.y}});
    centredText(target, assets.bodyBold, ">", 20, theme::Blue,
        {selector.position + sf::Vector2f{selector.size.x - ArrowWidth, 0.0F},
            {ArrowWidth, selector.size.y}});
}

/// One action: its name, then the key (and the gamepad button, if bound) in keycaps,
/// or a prompt while waiting for a key or a button.
void keyRow(sf::RenderTarget& target, const Assets& assets, int row, std::string_view action,
    sf::Keyboard::Key key, std::optional<unsigned> button, bool focused, bool waiting)
{
    const sf::FloatRect rect = layout::settingsRow(row);
    rowFocus(target, rect, focused);
    text(target, assets.body, action, 20, theme::Text, rect.position + sf::Vector2f{12.0F, 12.0F});
    const float right = rect.position.x + rect.size.x - 4.0F;
    if (waiting) {
        constexpr std::string_view Prompt = "Key or button (Esc: cancel)";
        text(target, assets.bodyBold, Prompt, 16, theme::Blue,
            {right - 8.0F - textWidth(assets.bodyBold, Prompt, 16), rect.position.y + 14.0F});
        return;
    }
    const sf::FloatRect keycap{{right - 120.0F, rect.position.y + 4.0F}, {120.0F, 36.0F}};
    box(target, keycap, theme::InputBackground, theme::Hull, 1.0F);
    centredText(target, assets.body, keyName(key).value_or("?"), 18, theme::Text, keycap);
    if (button) {
        const sf::FloatRect padcap{{keycap.position.x - 104.0F, keycap.position.y}, {96.0F, 36.0F}};
        box(target, padcap, theme::InputBackground, theme::Hull, 1.0F);
        centredText(target, assets.body, "Pad " + std::to_string(*button), 18, theme::Text, padcap);
    }
}

void drawSettings(sf::RenderTarget& target, const Assets& assets, const Session& session,
    const Settings& settings, sf::FloatRect area)
{
    box(target, area, theme::Overlay, theme::Overlay, 0.0F);
    widgets::panel(target, layout::SettingsPanel, theme::Surface, theme::Hull, 1.5F, 18.0F);
    text(target, assets.display, "Settings", 40, theme::Text, {261.0F, 132.0F});
    volumeRow(target, assets, settings, session.focus == layout::VolumeRow);
    selectorRow(target, assets, layout::ColourRow, "Colour-blind mode", layout::ColourSelector,
        colourModeName(settings.colourMode), session.focus == layout::ColourRow);
    selectorRow(target, assets, layout::InterfaceSizeRow, "Interface size",
        layout::InterfaceSizeSelector, std::to_string(settings.interfaceSize) + " %",
        session.focus == layout::InterfaceSizeRow);

    box(target, {{640.0F, 184.0F}, {1.0F, 312.0F}}, theme::Hull, theme::Hull, 0.0F);
    text(target, assets.display, "Key bindings", 32, theme::Text, {662.0F, 186.0F});
    constexpr std::array<std::string_view, ActionCount> actions{
        "Move up",
        "Move down",
        "Move left",
        "Move right",
        "Fire",
    };
    for (std::size_t action = 0; action < ActionCount; ++action) {
        const int row = layout::FirstKeyRow + static_cast<int>(action);
        keyRow(target, assets, row, actions.at(action), settings.keys.at(action),
            settings.buttons.at(action), session.focus == row,
            session.capturing == static_cast<Action>(action));
    }
    widgets::button(target, assets, layout::settingsRow(layout::ResetRow), "Reset controls", false,
        session.focus == layout::ResetRow);
    widgets::button(target, assets, layout::settingsRow(layout::BackRow), "Back", true,
        session.focus == layout::BackRow);

    if (!settings.saveError.empty()) {
        errorBox(target, assets, {{245.0F, 504.0F}, {790.0F, 32.0F}}, settings.saveError);
    }
    centredHelp(target, assets, "Arrows: move  ·  Enter: change  ·  Esc: back", 552.0F);
}

void drawGameOver(sf::RenderTarget& target, const Assets& assets, ColourMode mode,
    const Session& session, int interfaceSize)
{
    const bool compact = layout::isCompact(interfaceSize);
    if (!compact) {
        screenDecoration(target, assets);
    }
    centredTitle(target, assets, "GAME OVER", compact ? 44U : 80U, compact ? 122.0F : 54.0F);
    centredText(target, assets.body, "TEAM SCORE", 14, theme::Gold,
        {{0.0F, compact ? 172.0F : 146.0F}, {theme::UiWidth, 17.0F}});
    centredText(target, assets.bodyBold, formatScore(session.score), compact ? 28U : 40U,
        theme::Text,
        {{0.0F, compact ? 189.0F : 169.0F}, {theme::UiWidth, compact ? 36.0F : 53.0F}});

    // The crew: the players still in the game
    std::vector<std::size_t> crew;
    for (std::size_t i = 0; i < session.players.size(); ++i) {
        if (session.players.at(i).present) {
            crew.push_back(i);
        }
    }
    drawBanners(target, assets, mode, session, crew, compact ? 232.0F : 238.0F, interfaceSize);
    widgets::button(target, assets, layout::backToMenu(interfaceSize), "Back to menu", true,
        session.focus == 0);
}

} // namespace

InterfaceSystem::InterfaceSystem(sf::RenderWindow& window, const Assets& assets) noexcept
    : _window(window)
    , _assets(assets)
{
}

void InterfaceSystem::update(ecs::World& world, const ecs::Time& time)
{
    const Session& session = singleton<Session>(world);
    const Settings& settings = singleton<Settings>(world);
    const ColourMode mode = settings.colourMode;
    const int size = settings.interfaceSize;
    const sf::FloatRect area = layout::visibleArea(session, size);
    _window.setView(letterboxed(area, _window.getSize()));
    switch (session.screen) {
    case Screen::Connect:
        drawConnect(_window, _assets, mode, session, layout::isCompact(size));
        break;
    case Screen::Home:
        drawHome(_window, _assets, mode, session, size);
        if (!session.settingsOpen) {
            const bool open = layout::friendsOpen(session);
            const sf::FloatRect bar = layout::friendsBar(area, open);
            if (open) {
                drawFriendsOpen(_window, _assets, session, bar);
            } else {
                drawFriendsClosed(_window, _assets, session, bar);
            }
        }
        break;
    case Screen::WaitingRoom:
        drawWaitingRoom(_window, _assets, mode, session, size);
        break;
    case Screen::Game:
        drawHud(_window, _assets, world, session, time.elapsedSeconds(), area);
        if (!session.chat.empty() || session.chatOpen) {
            drawChat(_window, _assets, session, layout::gameChat(area), false);
        }
        if (session.menuOpen) {
            drawMenu(_window, _assets, session, area);
        }
        break;
    case Screen::GameOver:
        drawGameOver(_window, _assets, mode, session, size);
        break;
    }
    if (session.settingsOpen) {
        drawSettings(_window, _assets, session, settings, area);
    }
    _window.display();
}

} // namespace client
