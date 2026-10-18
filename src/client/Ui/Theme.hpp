#pragma once

#include <SFML/Graphics/Color.hpp>

#include <array>

/// The "Hangar" style of the mockups: light pixel-art panels, the exact
/// colours of the ship sprites (r-typesheet42.gif).
namespace client::theme {

// The interface is laid out on a 1280 × 720 canvas, the game on the
// 1920 × 1080 arena: both are scaled to the window, keeping 16:9.
inline constexpr float UiWidth = 1280.0F;
inline constexpr float UiHeight = 720.0F;

inline const sf::Color Background{0xE6, 0xEA, 0xEA};
inline const sf::Color Grid{82, 90, 90, 18};
inline const sf::Color Surface{0xFF, 0xFF, 0xFF};
inline const sf::Color InputBackground{0xF2, 0xF4, 0xF4};
inline const sf::Color Text{0x25, 0x29, 0x2B};
inline const sf::Color Muted{0x52, 0x5A, 0x5A};
inline const sf::Color Blue{0x00, 0x5A, 0x94};
inline const sf::Color DarkBlue{0x00, 0x3A, 0x60};
inline const sf::Color LightBlue{0x00, 0xE7, 0xDE};
inline const sf::Color Green{0x00, 0xA5, 0x00};
inline const sf::Color Red{0xCE, 0x00, 0x00};
inline const sf::Color ErrorBackground{0xFF, 0xE8, 0xE3};
inline const sf::Color Hull{0x52, 0x5A, 0x5A};
inline const sf::Color HullLight{0xA5, 0xA5, 0xA5};
inline const sf::Color GameBackground{0x06, 0x2A, 0x45};
inline const sf::Color Overlay{6, 42, 69, 209}; // GameBackground at 82 %
inline const sf::Color Letterbox{0x00, 0x00, 0x00};

/// P1-P4: the colour of each ship. Never alone: each also has a pattern and
/// a "P1"-"P4" tag, so colour-blind players can tell them apart.
inline const std::array<sf::Color, 4> Players{
    sf::Color{0x00, 0xB4, 0xC8}, // Light blue, stripes
    sf::Color{0xC6, 0x21, 0xC6}, // Pink, dots
    sf::Color{0x00, 0xA5, 0x00}, // Green, checks
    sf::Color{0xCE, 0x00, 0x00}, // Red, solid
};

} // namespace client::theme
