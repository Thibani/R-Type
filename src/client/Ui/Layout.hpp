#pragma once

#include "client/Components/Session.hpp"
#include "client/Ui/Theme.hpp"

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
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

// Main page: Play (0), Settings (1), Log out (2). The right of the screen stays free.
inline constexpr int PlayButton = 0;
inline constexpr int HomeSettingsButton = 1;
inline constexpr int LogOutButton = 2;

[[nodiscard]]
constexpr sf::FloatRect homeBanner(int interfaceSize) noexcept
{
    return isCompact(interfaceSize) ? sf::FloatRect{{554.0F, 130.0F}, {172.0F, 280.0F}}
                                    : sf::FloatRect{{528.0F, 130.0F}, {224.0F, 365.0F}};
}

[[nodiscard]]
constexpr sf::FloatRect homeMessage(int interfaceSize) noexcept
{
    return {{528.0F, isCompact(interfaceSize) ? 424.0F : 560.0F}, {224.0F, 40.0F}};
}

[[nodiscard]]
constexpr sf::FloatRect play(int interfaceSize) noexcept
{
    return {{528.0F, isCompact(interfaceSize) ? 474.0F : 620.0F}, {224.0F, 52.0F}};
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
    if (row == ResetRow) {
        return {{Left, 380.0F}, {Width, 52.0F}};
    }
    if (row == BackRow) {
        return {{Left, 444.0F}, {Width, 52.0F}};
    }
    return {{650.0F, 228.0F + (static_cast<float>(row - FirstKeyRow) * 54.0F)}, {385.0F, 44.0F}};
}

// Game over
[[nodiscard]]
constexpr sf::FloatRect backToMenu(int interfaceSize) noexcept
{
    return {{500.0F, isCompact(interfaceSize) ? 526.0F : 636.0F}, {280.0F, 52.0F}};
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

// Friends bar, on the right of the main page, whatever the interface size. In focus
// order after the buttons of the page: the name field, Add, then the buttons of the rows.
// Closed, the bar itself takes the place of the field: moving the focus to it opens it.
inline constexpr int FriendNameField = 3;
inline constexpr int FriendAddButton = 4;
inline constexpr int FirstFriendButton = 5;
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
    return session.screen == Screen::Home && !session.settingsOpen
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
    switch (session.screen) {
    case Screen::Connect:
        return {NameInput, PasswordInput, AddressInput, PortInput, Login, CreateAccount,
            OpenSettings, Quit};
    case Screen::Home: {
        std::vector<sf::FloatRect> parts{
            play(interfaceSize), homeSettings(interfaceSize), logOut(interfaceSize)};
        const sf::FloatRect area = visibleArea(session, interfaceSize);
        if (!friendsOpen(session)) {
            // One unit inside the bar, which itself touches the edges of the view
            const sf::FloatRect bar = friendsBar(area, false);
            parts.push_back(
                {bar.position + sf::Vector2f{1.0F, 1.0F}, bar.size - sf::Vector2f{2.0F, 2.0F}});
            return parts;
        }
        const sf::FloatRect bar = friendsBar(area, true);
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
        return {backToMenu(interfaceSize)};
    }
    return {};
}

} // namespace client::layout
