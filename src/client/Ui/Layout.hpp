#pragma once

#include "client/Components/Session.hpp"

#include <SFML/Graphics/Rect.hpp>

#include <vector>

/// Where the clickable parts of each screen are, on the 1280 × 720 interface
/// canvas (positions taken from the Hangar mockup). The input system uses
/// them for clicks, the interface system to draw: both always agree.
namespace client::layout {

// Connection form: fields and buttons, in Tab order (Session::focus).
inline constexpr int NameField = 0;
inline constexpr int AddressField = 1;
inline constexpr int PortField = 2;
inline constexpr int ConnectButton = 3;
inline constexpr int QuitButton = 4;

inline constexpr sf::FloatRect ConnectPanel{{668.0F, 132.0F}, {532.0F, 456.0F}};
inline constexpr sf::FloatRect NameInput{{700.0F, 245.0F}, {468.0F, 48.0F}};
inline constexpr sf::FloatRect AddressInput{{700.0F, 367.0F}, {306.0F, 48.0F}};
inline constexpr sf::FloatRect PortInput{{1024.0F, 367.0F}, {144.0F, 48.0F}};
inline constexpr sf::FloatRect Connect{{700.0F, 437.0F}, {340.0F, 56.0F}};
inline constexpr sf::FloatRect Quit{{1056.0F, 437.0F}, {112.0F, 56.0F}};

// Waiting room
inline constexpr sf::FloatRect Leave{{80.0F, 608.0F}, {112.0F, 56.0F}};

// Esc menu over the game: Resume (0), Leave game (1)
inline constexpr sf::FloatRect MenuPanel{{430.0F, 214.0F}, {420.0F, 292.0F}};
inline constexpr sf::FloatRect Resume{{462.0F, 342.0F}, {356.0F, 56.0F}};
inline constexpr sf::FloatRect LeaveGame{{462.0F, 416.0F}, {356.0F, 56.0F}};

// Game over
inline constexpr sf::FloatRect BackToMenu{{530.0F, 612.0F}, {220.0F, 56.0F}};

/// The focusable parts of the current screen, in focus order.
[[nodiscard]]
inline std::vector<sf::FloatRect> focusables(const Session& session)
{
    switch (session.screen) {
    case Screen::Connect:
        return {NameInput, AddressInput, PortInput, Connect, Quit};
    case Screen::WaitingRoom:
        return {Leave};
    case Screen::Game:
        if (session.menuOpen) {
            return {Resume, LeaveGame};
        }
        return {};
    case Screen::GameOver:
        return {BackToMenu};
    }
    return {};
}

} // namespace client::layout
