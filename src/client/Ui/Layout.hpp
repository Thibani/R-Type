#pragma once

#include "client/Components/Session.hpp"
#include "client/Ui/Theme.hpp"

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

/// Where the clickable parts of each screen are, on the 1280 × 720 interface
/// canvas (positions taken from the Figma mockup). The input system uses
/// them for clicks, the interface system to draw: both always agree.
///
/// Interface size: positions stay on this canvas, and the view shows more or
/// less of it (`visibleArea()`). Above 100 %, screens too wide for that use
/// their compact form.
namespace client::layout {

[[nodiscard]]
constexpr bool isCompact(int interfaceSize) noexcept
{
    return interfaceSize > 100;
}

// Connection form: fields and buttons, in Tab order (Session::focus).
inline constexpr int NameField = 0;
inline constexpr int PasswordField = 1;
inline constexpr int AddressField = 2;
inline constexpr int PortField = 3;
inline constexpr int LoginButton = 4;
inline constexpr int CreateAccountButton = 5;
inline constexpr int SettingsButton = 6;
inline constexpr int QuitButton = 7;

inline constexpr sf::FloatRect ConnectPanel{{684.0F, 124.0F}, {532.0F, 472.0F}};
inline constexpr sf::FloatRect NameInput{{716.0F, 236.0F}, {226.0F, 44.0F}};
inline constexpr sf::FloatRect PasswordInput{{958.0F, 236.0F}, {226.0F, 44.0F}};
inline constexpr sf::FloatRect AddressInput{{716.0F, 318.0F}, {306.0F, 44.0F}};
inline constexpr sf::FloatRect PortInput{{1040.0F, 318.0F}, {144.0F, 44.0F}};
inline constexpr sf::Vector2f ConnectStatus{716.0F, 374.0F}; // Between the fields and the buttons
inline constexpr float ConnectStatusWidth = 468.0F;
inline constexpr sf::FloatRect Login{{716.0F, 402.0F}, {226.0F, 52.0F}};
inline constexpr sf::FloatRect CreateAccount{{958.0F, 402.0F}, {226.0F, 52.0F}};
inline constexpr sf::FloatRect OpenSettings{{716.0F, 466.0F}, {226.0F, 52.0F}};
inline constexpr sf::FloatRect Quit{{958.0F, 466.0F}, {226.0F, 52.0F}};

// Main page: Play (0), Settings (1), Log out (2), the chat, the group, Leaderboard, then
// the friends.
// A part that does not apply for now (Kick, for who does not lead) keeps its number, with
// an empty rectangle: nothing can click it, and the focus goes past it.
inline constexpr int PlayButton = 0;
inline constexpr int HomeSettingsButton = 1;
inline constexpr int LogOutButton = 2;
inline constexpr int NewGroupCodeButton = 4;
inline constexpr int DeleteGroupCodeButton = 5;
inline constexpr int GroupCodeField = 6;
inline constexpr int GroupJoinButton = 7;
inline constexpr int LeaveGroupButton = 8;
inline constexpr int AcceptInvitationButton = 9;
inline constexpr int RefuseInvitationButton = 10;
inline constexpr int FirstKickButton = 11; // + place of the member
inline constexpr int FirstPromoteButton = 15; // + place of the member
inline constexpr int LeaderboardButton = 19;

/// The left of the group's part of the page.
[[nodiscard]]
constexpr float groupLeft(int interfaceSize) noexcept
{
    return isCompact(interfaceSize) ? 554.0F : 528.0F;
}

/// What the group has to say: an invitation, or an error. Short enough, at normal
/// sizes, for the open friends panel not to hide its buttons.
[[nodiscard]]
constexpr sf::FloatRect homeMessage(int interfaceSize) noexcept
{
    return {{groupLeft(interfaceSize), isCompact(interfaceSize) ? 416.0F : 548.0F},
        {isCompact(interfaceSize) ? 450.0F : 440.0F, 44.0F}};
}

[[nodiscard]]
constexpr sf::FloatRect play(int interfaceSize) noexcept
{
    return {
        {groupLeft(interfaceSize), isCompact(interfaceSize) ? 474.0F : 620.0F}, {220.0F, 52.0F}};
}

[[nodiscard]]
constexpr sf::FloatRect leaveGroup(int interfaceSize) noexcept
{
    return {{groupLeft(interfaceSize) + 228.0F, isCompact(interfaceSize) ? 474.0F : 620.0F},
        {220.0F, 52.0F}};
}

[[nodiscard]]
constexpr sf::Vector2f groupCodeLabel(int interfaceSize) noexcept
{
    return {groupLeft(interfaceSize), (isCompact(interfaceSize) ? 130.0F : 104.0F) + 6.0F};
}

[[nodiscard]]
constexpr sf::FloatRect newGroupCode(int interfaceSize) noexcept
{
    return isCompact(interfaceSize) ? sf::FloatRect{{680.0F, 130.0F}, {100.0F, 36.0F}}
                                    : sf::FloatRect{{680.0F, 104.0F}, {104.0F, 40.0F}};
}

[[nodiscard]]
constexpr sf::FloatRect deleteGroupCode(int interfaceSize) noexcept
{
    return isCompact(interfaceSize) ? sf::FloatRect{{786.0F, 130.0F}, {70.0F, 36.0F}}
                                    : sf::FloatRect{{790.0F, 104.0F}, {74.0F, 40.0F}};
}

[[nodiscard]]
constexpr sf::FloatRect groupCodeInput(int interfaceSize) noexcept
{
    return isCompact(interfaceSize) ? sf::FloatRect{{554.0F, 194.0F}, {160.0F, 36.0F}}
                                    : sf::FloatRect{{880.0F, 104.0F}, {160.0F, 40.0F}};
}

[[nodiscard]]
constexpr sf::FloatRect groupJoin(int interfaceSize) noexcept
{
    return isCompact(interfaceSize) ? sf::FloatRect{{720.0F, 194.0F}, {70.0F, 36.0F}}
                                    : sf::FloatRect{{1046.0F, 104.0F}, {70.0F, 40.0F}};
}

/// The card of the member at `place`, side by side.
[[nodiscard]]
constexpr sf::FloatRect memberCard(int interfaceSize, std::size_t place) noexcept
{
    const auto index = static_cast<float>(place);
    return isCompact(interfaceSize)
        ? sf::FloatRect{{554.0F + (index * 114.0F), 238.0F}, {106.0F, 170.0F}}
        : sf::FloatRect{{528.0F + (index * 172.0F), 170.0F}, {160.0F, 340.0F}};
}

[[nodiscard]]
constexpr sf::FloatRect kickMember(int interfaceSize, std::size_t place) noexcept
{
    const sf::FloatRect card = memberCard(interfaceSize, place);
    return isCompact(interfaceSize)
        ? sf::FloatRect{card.position + sf::Vector2f{8.0F, 110.0F}, {42.0F, 28.0F}}
        : sf::FloatRect{card.position + sf::Vector2f{16.0F, 186.0F}, {60.0F, 30.0F}};
}

[[nodiscard]]
constexpr sf::FloatRect promoteMember(int interfaceSize, std::size_t place) noexcept
{
    const sf::FloatRect card = memberCard(interfaceSize, place);
    return isCompact(interfaceSize)
        ? sf::FloatRect{card.position + sf::Vector2f{56.0F, 110.0F}, {42.0F, 28.0F}}
        : sf::FloatRect{card.position + sf::Vector2f{84.0F, 186.0F}, {60.0F, 30.0F}};
}

/// Answering the oldest invitation, on the right of the message.
[[nodiscard]]
constexpr sf::FloatRect refuseInvitation(int interfaceSize) noexcept
{
    const sf::FloatRect line = homeMessage(interfaceSize);
    return {{line.position.x + line.size.x - 70.0F, line.position.y + 7.0F}, {62.0F, 30.0F}};
}

[[nodiscard]]
constexpr sf::FloatRect acceptInvitation(int interfaceSize) noexcept
{
    const sf::FloatRect line = refuseInvitation(interfaceSize);
    return {{line.position.x - 68.0F, line.position.y}, line.size};
}

[[nodiscard]]
constexpr sf::FloatRect homeSettings(int interfaceSize) noexcept
{
    return {{isCompact(interfaceSize) ? 240.0F : 64.0F, isCompact(interfaceSize) ? 410.0F : 556.0F},
        {224.0F, 52.0F}};
}

[[nodiscard]]
constexpr sf::FloatRect logOut(int interfaceSize) noexcept
{
    return {{isCompact(interfaceSize) ? 240.0F : 64.0F, isCompact(interfaceSize) ? 474.0F : 620.0F},
        {224.0F, 52.0F}};
}

/// Under the chat, above Settings.
[[nodiscard]]
constexpr sf::FloatRect openLeaderboard(int interfaceSize) noexcept
{
    return {{isCompact(interfaceSize) ? 240.0F : 64.0F, isCompact(interfaceSize) ? 346.0F : 492.0F},
        {224.0F, 52.0F}};
}

// Leaderboard, over the main page. It fits in the view of the largest interface size.
inline constexpr int SoloTab = 0;
inline constexpr int MultiTab = 1;
inline constexpr int CloseLeaderboardButton = 2;
inline constexpr sf::FloatRect LeaderboardPanel{{300.0F, 150.0F}, {680.0F, 410.0F}};
inline constexpr sf::FloatRect SoloTabRect{{742.0F, 162.0F}, {110.0F, 30.0F}};
inline constexpr sf::FloatRect MultiTabRect{{858.0F, 162.0F}, {110.0F, 30.0F}};
inline constexpr sf::FloatRect CloseLeaderboard{{528.0F, 494.0F}, {224.0F, 52.0F}};
inline constexpr float LeaderboardRowsTop = 206.0F;
inline constexpr float LeaderboardRowHeight = 24.0F;
inline constexpr unsigned LeaderboardTextSize = 14;

/// Line `row` of the ranking (0 to 9), then the player's last game (10).
[[nodiscard]]
constexpr sf::FloatRect leaderboardRow(std::size_t row) noexcept
{
    const float gap = row >= network::protocol::LeaderboardSize ? 10.0F : 0.0F;
    return {{316.0F, LeaderboardRowsTop + gap + (static_cast<float>(row) * LeaderboardRowHeight)},
        {648.0F, LeaderboardRowHeight}};
}

/// Open over the main page only: entering a game hides it.
[[nodiscard]]
constexpr bool leaderboardOpen(const Session& session) noexcept
{
    return session.screen == Screen::Home && session.leaderboardOpen && !session.settingsOpen;
}

// Waiting room and game over: up to 4 banners side by side
[[nodiscard]]
constexpr sf::Vector2f bannerSize(int interfaceSize) noexcept
{
    return isCompact(interfaceSize) ? sf::Vector2f{172.0F, 280.0F} : sf::Vector2f{224.0F, 365.0F};
}

[[nodiscard]]
constexpr float bannerGap(int interfaceSize) noexcept
{
    return isCompact(interfaceSize) ? 16.0F : 24.0F;
}

[[nodiscard]]
constexpr sf::FloatRect leave(int interfaceSize) noexcept
{
    return {{528.0F, isCompact(interfaceSize) ? 474.0F : 620.0F}, {224.0F, 52.0F}};
}

// In the game
inline constexpr sf::Vector2f PlayerCard{151.0F, 92.0F};

// Esc menu over the game: Resume (0), Settings (1), Leave game (2)
inline constexpr int ResumeButton = 0;
inline constexpr int MenuSettingsButton = 1;
inline constexpr int LeaveButton = 2;
inline constexpr sf::FloatRect MenuPanel{{420.0F, 155.0F}, {440.0F, 410.0F}};
inline constexpr sf::FloatRect Resume{{460.0F, 305.0F}, {360.0F, 52.0F}};
inline constexpr sf::FloatRect MenuSettings{{460.0F, 373.0F}, {360.0F, 52.0F}};
inline constexpr sf::FloatRect LeaveGame{{460.0F, 441.0F}, {360.0F, 52.0F}};

// Settings, over any screen, on two columns so they fit at every size. Left:
// volume, colours, interface size and two buttons. Right: one row per action.
// Rows keep the focus order.
inline constexpr int VolumeRow = 0;
inline constexpr int ColourRow = 1;
inline constexpr int InterfaceSizeRow = 2;
inline constexpr int FirstKeyRow = 3; // + Action index
inline constexpr int ResetRow = FirstKeyRow + static_cast<int>(ActionCount);
inline constexpr int BackRow = ResetRow + 1;
inline constexpr sf::FloatRect SettingsPanel{{223.0F, 124.0F}, {834.0F, 472.0F}};
inline constexpr sf::FloatRect VolumeTrack{{261.0F, 238.0F}, {353.0F, 4.0F}};
inline constexpr sf::FloatRect ColourSelector{{434.0F, 268.0F}, {180.0F, 40.0F}};
inline constexpr sf::FloatRect InterfaceSizeSelector{{434.0F, 322.0F}, {180.0F, 40.0F}};

[[nodiscard]]
constexpr sf::FloatRect settingsRow(int row) noexcept
{
    constexpr float Left = 245.0F;
    constexpr float Width = 385.0F;
    if (row == VolumeRow) {
        return {{Left, 178.0F}, {Width, 80.0F}};
    }
    if (row == ColourRow) {
        return {{Left, 262.0F}, {Width, 52.0F}};
    }
    if (row == InterfaceSizeRow) {
        return {{Left, 316.0F}, {Width, 52.0F}};
    }
    // Reset and Back end the left column on the last two rows of the right one
    if (row == ResetRow) {
        return {{Left, 442.0F}, {Width, 48.0F}};
    }
    if (row == BackRow) {
        return {{Left, 496.0F}, {Width, 48.0F}};
    }
    // One row per action, 6 of them since the Force
    return {{650.0F, 228.0F + (static_cast<float>(row - FirstKeyRow) * 54.0F)}, {385.0F, 44.0F}};
}

// Game over: "Play again" first, so Enter starts a new game; "Back to menu" returns to the
// main page
inline constexpr int PlayAgainButton = 0;
inline constexpr int BackToMenuButton = 1;

[[nodiscard]]
constexpr sf::FloatRect playAgain(int interfaceSize) noexcept
{
    return {{410.0F, isCompact(interfaceSize) ? 526.0F : 612.0F}, {220.0F, 56.0F}};
}

/// Beside Play again; alone and centred after the game of a group.
[[nodiscard]]
constexpr sf::FloatRect backToMenu(int interfaceSize, bool beside = true) noexcept
{
    return {
        {beside ? 650.0F : 530.0F, isCompact(interfaceSize) ? 526.0F : 612.0F}, {220.0F, 56.0F}};
}

/// The part of the canvas shown at this interface size, around what the screen
/// needs. Larger than the canvas below 100 %, smaller above.
[[nodiscard]]
constexpr sf::FloatRect visibleArea(const Session& session, int interfaceSize) noexcept
{
    const float ratio = 100.0F / static_cast<float>(interfaceSize);
    const sf::Vector2f size{theme::UiWidth * ratio, theme::UiHeight * ratio};
    sf::Vector2f centre{theme::UiWidth / 2.0F, theme::UiHeight / 2.0F};
    if (isCompact(interfaceSize) && !session.settingsOpen && session.screen == Screen::Connect) {
        centre.x = ConnectPanel.position.x + (ConnectPanel.size.x / 2.0F);
    }
    return {{centre.x - (size.x / 2.0F), centre.y - (size.y / 2.0F)}, size};
}

/// The focusable nearest to `rects[from]` in direction (`dx`, `dy`), one of
/// them 0 and the other -1 or 1. Controls in line with it come first (their
/// sides overlap), nearest first: Down from a field picks the field just
/// below, Right from a row the row beside it. Otherwise the closest that way.
/// `from` itself when nothing lies that way: the focus stops at the edges.
[[nodiscard]]
inline int neighbour(const std::vector<sf::FloatRect>& rects, int from, int dx, int dy)
{
    if (from < 0 || std::cmp_greater_equal(from, rects.size())) {
        return rects.empty() ? from : 0;
    }
    const sf::FloatRect current = rects.at(static_cast<std::size_t>(from));
    const auto inLine = [&](sf::FloatRect other) {
        const bool sideways = dx != 0;
        const float start = sideways ? current.position.y : current.position.x;
        const float end = start + (sideways ? current.size.y : current.size.x);
        const float otherStart = sideways ? other.position.y : other.position.x;
        const float otherEnd = otherStart + (sideways ? other.size.y : other.size.x);
        return otherStart < end && start < otherEnd;
    };
    int best = from;
    std::tuple<bool, float, float> bestRank{};
    for (std::size_t i = 0; i < rects.size(); ++i) {
        const sf::Vector2f offset = rects.at(i).getCenter() - current.getCenter();
        const float along
            = (offset.x * static_cast<float>(dx)) + (offset.y * static_cast<float>(dy));
        if (along <= 1.0F) {
            continue; // Not that way
        }
        const float across = std::abs(dx != 0 ? offset.y : offset.x);
        const bool aside = !inLine(rects.at(i));
        // In line first, then nearest, then the most directly that way
        const std::tuple rank{aside, aside ? along + (2.0F * across) : along, across};
        if (best == from || rank < bestRank) {
            best = static_cast<int>(i);
            bestRank = rank;
        }
    }
    return best;
}

// Chat box, on the main page (above Leaderboard and Settings), and in the game (bottom left,
// above the reminder of the keys). On the main page, it is the 4th focusable part.
inline constexpr int ChatBox = 3;
inline constexpr float ChatInputHeight = 26.0F;
inline constexpr float ChatLineHeight = 17.0F;
inline constexpr unsigned ChatTextSize = 13;
// In the game, only the last messages show, so that the game stays visible; scrolling back
// still reaches the others
inline constexpr std::size_t GameChatMessages = 3;

[[nodiscard]]
constexpr sf::FloatRect homeChat(int interfaceSize) noexcept
{
    return isCompact(interfaceSize) ? sf::FloatRect{{225.0F, 130.0F}, {315.0F, 204.0F}}
                                    : sf::FloatRect{{64.0F, 282.0F}, {440.0F, 194.0F}};
}

[[nodiscard]]
constexpr sf::FloatRect gameChat(sf::FloatRect area) noexcept
{
    // Three lines and the typing line
    constexpr sf::Vector2f Size{420.0F, 16.0F + (3.0F * ChatLineHeight) + ChatInputHeight + 4.0F};
    return {{area.position.x + 18.0F, area.position.y + area.size.y - 60.0F - Size.y}, Size};
}

// Friends bar, on the right of the main page, whatever the interface size. In focus
// order after the chat box: the name field, Add, then the buttons of the rows.
// Closed, the bar itself takes the place of the field: moving the focus to it opens it.
inline constexpr int FriendNameField = 20;
inline constexpr int FriendAddButton = 21;
inline constexpr int FirstFriendButton = 22;
inline constexpr float FriendsClosedWidth = 56.0F;
inline constexpr float FriendsOpenWidth = 300.0F;
inline constexpr float FriendsPadding = 16.0F;
inline constexpr float FriendRowsTop = 158.0F; // From the top of the bar
inline constexpr float FriendRowHeight = 50.0F;
inline constexpr float FriendsBottomLine = 28.0F; // "PgUp / PgDn" under the rows

/// Open while the mouse is over it, or while the focus is inside.
[[nodiscard]]
constexpr bool friendsOpen(const Session& session) noexcept
{
    return session.screen == Screen::Home && !session.settingsOpen && !session.leaderboardOpen
        && (session.friendsHovered || session.focus >= FriendNameField);
}

[[nodiscard]]
constexpr sf::FloatRect friendsBar(sf::FloatRect area, bool open) noexcept
{
    const float width = open ? FriendsOpenWidth : FriendsClosedWidth;
    return {{area.position.x + area.size.x - width, area.position.y}, {width, area.size.y}};
}

[[nodiscard]]
constexpr sf::FloatRect friendNameInput(sf::FloatRect bar) noexcept
{
    return {bar.position + sf::Vector2f{FriendsPadding, 76.0F}, {192.0F, 44.0F}};
}

[[nodiscard]]
constexpr sf::FloatRect friendAdd(sf::FloatRect bar) noexcept
{
    return {bar.position + sf::Vector2f{FriendsPadding + 200.0F, 76.0F}, {68.0F, 44.0F}};
}

/// How many rows fit in this bar (at least one).
[[nodiscard]]
constexpr std::size_t friendRowsShown(sf::FloatRect bar) noexcept
{
    const float room = bar.size.y - FriendRowsTop - FriendsBottomLine;
    return room < FriendRowHeight ? 1 : static_cast<std::size_t>(room / FriendRowHeight);
}

[[nodiscard]]
constexpr sf::FloatRect friendRow(sf::FloatRect bar, std::size_t rowOnScreen) noexcept
{
    return {bar.position
            + sf::Vector2f{0.0F,
                FriendRowsTop + (static_cast<float>(rowOnScreen) * FriendRowHeight)},
        {bar.size.x, FriendRowHeight}};
}

/// What a button of a row does.
enum class FriendAction : std::uint8_t { Accept, Refuse, Remove };

struct FriendButton {
    sf::FloatRect rect;
    FriendAction action{FriendAction::Remove};
    std::size_t row{0}; // In `Session::friends`
};

/// The buttons of the rows on screen, in focus order. Depends on the state of each row.
[[nodiscard]]
inline std::vector<FriendButton> friendButtons(const Session& session, sf::FloatRect bar)
{
    using network::protocol::FriendState;
    constexpr sf::Vector2f Size{62.0F, 30.0F};
    const auto last = std::min(session.friends.size(), session.friendScroll + friendRowsShown(bar));
    std::vector<FriendButton> buttons;
    for (std::size_t row = session.friendScroll; row < last; ++row) {
        const sf::FloatRect line = friendRow(bar, row - session.friendScroll);
        const sf::Vector2f right{
            line.position.x + line.size.x - FriendsPadding - Size.x, line.position.y + 10.0F};
        if (session.friends.at(row).state == FriendState::RequestReceived) {
            buttons.push_back(
                {{right - sf::Vector2f{Size.x + 6.0F, 0.0F}, Size}, FriendAction::Accept, row});
            buttons.push_back({{right, Size}, FriendAction::Refuse, row});
        } else {
            buttons.push_back({{right, Size}, FriendAction::Remove, row});
        }
    }
    return buttons;
}

/// The Message button of the right-click menu, just under the row of `session.friendMenu`,
/// so that its name stays visible. It stays inside the bar, even under the last row.
/// Nullopt if no menu is open, or if that row is not on screen.
[[nodiscard]]
inline std::optional<sf::FloatRect> friendMenuButton(const Session& session, sf::FloatRect bar)
{
    if (!session.friendMenu) {
        return std::nullopt;
    }
    const auto last = std::min(session.friends.size(), session.friendScroll + friendRowsShown(bar));
    for (std::size_t row = session.friendScroll; row < last; ++row) {
        if (session.friends.at(row).name == *session.friendMenu) {
            const sf::FloatRect line = friendRow(bar, row - session.friendScroll);
            constexpr sf::Vector2f Size{96.0F, 34.0F};
            const float lowest = bar.position.y + bar.size.y - Size.y - 2.0F;
            return sf::FloatRect{{line.position.x + FriendsPadding,
                                     std::min(line.position.y + line.size.y - 6.0F, lowest)},
                Size};
        }
    }
    return std::nullopt;
}

/// The Invite button of the same menu, beside Message: for the leader, and a friend online.
[[nodiscard]]
inline std::optional<sf::FloatRect> friendInviteButton(const Session& session, sf::FloatRect bar)
{
    const auto message = friendMenuButton(session, bar);
    const auto person = std::ranges::find(session.friends, session.friendMenu,
        [](const FriendRow& row) { return std::optional<std::string>{row.name}; });
    if (!message || person == session.friends.end() || !leadsTheGroup(session)
        || person->state != network::protocol::FriendState::Online) {
        return std::nullopt;
    }
    return sf::FloatRect{
        message->position + sf::Vector2f{message->size.x + 6.0F, 0.0F}, message->size};
}

/// The chat box of the current screen. Only the main page and the game have one.
[[nodiscard]]
constexpr sf::FloatRect chatBox(
    const Session& session, sf::FloatRect area, int interfaceSize) noexcept
{
    return session.screen == Screen::Home ? homeChat(interfaceSize) : gameChat(area);
}

/// The focusable parts of the current screen, in focus order.
[[nodiscard]]
inline std::vector<sf::FloatRect> focusables(const Session& session, int interfaceSize)
{
    if (session.settingsOpen) {
        std::vector<sf::FloatRect> rows;
        for (int row = 0; row <= BackRow; ++row) {
            rows.push_back(settingsRow(row));
        }
        return rows;
    }
    if (leaderboardOpen(session)) {
        return {SoloTabRect, MultiTabRect, CloseLeaderboard};
    }
    switch (session.screen) {
    case Screen::Connect:
        return {NameInput, PasswordInput, AddressInput, PortInput, Login, CreateAccount,
            OpenSettings, Quit};
    case Screen::Home: {
        const auto when
            = [](bool applies, sf::FloatRect rect) { return applies ? rect : sf::FloatRect{}; };
        const bool leads = leadsTheGroup(session);
        const std::size_t members = session.group.members.size();
        const bool invited = !session.invitations.empty();
        std::vector<sf::FloatRect> parts{play(interfaceSize), homeSettings(interfaceSize),
            logOut(interfaceSize), homeChat(interfaceSize),
            when(leads, newGroupCode(interfaceSize)),
            when(leads && !session.group.code.empty(), deleteGroupCode(interfaceSize)),
            groupCodeInput(interfaceSize), groupJoin(interfaceSize),
            when(members >= 2, leaveGroup(interfaceSize)),
            when(invited, acceptInvitation(interfaceSize)),
            when(invited, refuseInvitation(interfaceSize))};
        const auto canActOn = [&](std::size_t place) {
            return leads && place < members && place != session.group.leader;
        };
        for (std::size_t place = 0; place < network::protocol::MaxPlayers; ++place) {
            parts.push_back(when(canActOn(place), kickMember(interfaceSize, place)));
        }
        for (std::size_t place = 0; place < network::protocol::MaxPlayers; ++place) {
            parts.push_back(when(canActOn(place), promoteMember(interfaceSize, place)));
        }
        parts.push_back(openLeaderboard(interfaceSize));
        const sf::FloatRect area = visibleArea(session, interfaceSize);
        if (!friendsOpen(session)) {
            // One unit inside the bar, which itself touches the edges of the view
            const sf::FloatRect bar = friendsBar(area, false);
            parts.push_back(
                {bar.position + sf::Vector2f{1.0F, 1.0F}, bar.size - sf::Vector2f{2.0F, 2.0F}});
            return parts;
        }
        const sf::FloatRect bar = friendsBar(area, true);
        // The open panel covers what is behind it: a click there is for the panel
        for (sf::FloatRect& part : parts) {
            if (part.findIntersection(bar)) {
                part = {};
            }
        }
        parts.push_back(friendNameInput(bar));
        parts.push_back(friendAdd(bar));
        for (const FriendButton& button : friendButtons(session, bar)) {
            parts.push_back(button.rect);
        }
        return parts;
    }
    case Screen::WaitingRoom:
        return {leave(interfaceSize)};
    case Screen::Game:
        if (session.menuOpen) {
            return {Resume, MenuSettings, LeaveGame};
        }
        return {};
    case Screen::GameOver:
        if (!playsAlone(session)) { // Play again does not apply: its number stays, empty
            return {sf::FloatRect{}, backToMenu(interfaceSize, false)};
        }
        return {playAgain(interfaceSize), backToMenu(interfaceSize)};
    }
    return {};
}

} // namespace client::layout
