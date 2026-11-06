#pragma once

#include "client/Components/Session.hpp"
#include "client/Ui/Theme.hpp"

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

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
    case Screen::Home:
        return {play(interfaceSize), homeSettings(interfaceSize), logOut(interfaceSize)};
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
