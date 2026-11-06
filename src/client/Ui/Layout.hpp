#pragma once

#include "client/Components/Session.hpp"
#include "client/Ui/Theme.hpp"

#include <SFML/Graphics/Rect.hpp>

#include <vector>

/// Where the clickable parts of each screen are, on the 1280 × 720 interface
/// canvas (positions taken from the Hangar mockup). The input system uses
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

inline constexpr sf::FloatRect ConnectPanel{{668.0F, 132.0F}, {532.0F, 456.0F}};
inline constexpr sf::FloatRect NameInput{{700.0F, 218.0F}, {226.0F, 48.0F}};
inline constexpr sf::FloatRect PasswordInput{{942.0F, 218.0F}, {226.0F, 48.0F}};
inline constexpr sf::FloatRect AddressInput{{700.0F, 324.0F}, {306.0F, 48.0F}};
inline constexpr sf::FloatRect PortInput{{1024.0F, 324.0F}, {144.0F, 48.0F}};
inline constexpr sf::FloatRect Login{{700.0F, 384.0F}, {226.0F, 56.0F}};
inline constexpr sf::FloatRect CreateAccount{{942.0F, 384.0F}, {226.0F, 56.0F}};
inline constexpr sf::FloatRect OpenSettings{{700.0F, 448.0F}, {226.0F, 56.0F}};
inline constexpr sf::FloatRect Quit{{942.0F, 448.0F}, {226.0F, 56.0F}};

// Waiting room
[[nodiscard]]
constexpr sf::FloatRect leave(int interfaceSize) noexcept
{
    return {isCompact(interfaceSize) ? sf::Vector2f{270.0F, 400.0F} : sf::Vector2f{80.0F, 608.0F},
        {112.0F, 56.0F}};
}

// Esc menu over the game: Resume (0), Settings (1), Leave game (2)
inline constexpr int ResumeButton = 0;
inline constexpr int MenuSettingsButton = 1;
inline constexpr int LeaveButton = 2;
inline constexpr sf::FloatRect MenuPanel{{430.0F, 214.0F}, {420.0F, 366.0F}};
inline constexpr sf::FloatRect Resume{{462.0F, 342.0F}, {356.0F, 56.0F}};
inline constexpr sf::FloatRect MenuSettings{{462.0F, 416.0F}, {356.0F, 56.0F}};
inline constexpr sf::FloatRect LeaveGame{{462.0F, 490.0F}, {356.0F, 56.0F}};

// Settings, over any screen, on two columns so they fit at every size. Left:
// volume, colours, interface size and two buttons. Right: one row per action.
inline constexpr int VolumeRow = 0;
inline constexpr int ColourRow = 1;
inline constexpr int InterfaceSizeRow = 2;
inline constexpr int FirstKeyRow = 3; // + Action index
inline constexpr int ResetRow = FirstKeyRow + static_cast<int>(ActionCount);
inline constexpr int BackRow = ResetRow + 1;
inline constexpr sf::FloatRect SettingsPanel{{223.0F, 124.0F}, {834.0F, 420.0F}};

[[nodiscard]]
constexpr sf::FloatRect settingsRow(int row) noexcept
{
    const auto at = [](float x, int line) -> sf::FloatRect {
        return {{x, 184.0F + (static_cast<float>(line) * 52.0F)}, {385.0F, 46.0F}};
    };
    if (row < FirstKeyRow) {
        return at(245.0F, row);
    }
    if (row < ResetRow) {
        return at(650.0F, row - FirstKeyRow);
    }
    return at(245.0F, row - ResetRow + FirstKeyRow);
}

// Game over
inline constexpr sf::FloatRect BackToMenu{{530.0F, 612.0F}, {220.0F, 56.0F}};

/// The part of the canvas shown at this interface size, around what the screen
/// needs. Larger than the canvas below 100 %, smaller above.
[[nodiscard]]
constexpr sf::FloatRect visibleArea(const Session& session, int interfaceSize) noexcept
{
    const float ratio = 100.0F / static_cast<float>(interfaceSize);
    const sf::Vector2f size{theme::UiWidth * ratio, theme::UiHeight * ratio};
    sf::Vector2f centre{theme::UiWidth / 2.0F, theme::UiHeight / 2.0F};
    if (isCompact(interfaceSize) && !session.settingsOpen) {
        if (session.screen == Screen::Connect) {
            centre.x = ConnectPanel.position.x + (ConnectPanel.size.x / 2.0F);
        } else if (session.screen == Screen::GameOver) {
            centre.y = 440.0F; // From the title to the button
        }
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
    case Screen::WaitingRoom:
        return {leave(interfaceSize)};
    case Screen::Game:
        if (session.menuOpen) {
            return {Resume, MenuSettings, LeaveGame};
        }
        return {};
    case Screen::GameOver:
        return {BackToMenu};
    }
    return {};
}

} // namespace client::layout
