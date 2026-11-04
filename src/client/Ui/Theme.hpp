#pragma once

#include "client/Components/Settings.hpp"

#include <SFML/Graphics/Color.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

/// The "Hangar" style of the mockups: light pixel-art panels, the exact
/// colours of the ship sprites (r-typesheet42.gif).
namespace client::theme {

// The interface is laid out on a 1280 × 720 canvas, the game on the
// 1920 × 1080 arena: both are scaled to the window, keeping 16:9.
inline constexpr float UiWidth = 1280.0F;
inline constexpr float UiHeight = 720.0F;

inline constexpr sf::Color Background{0xE6, 0xEA, 0xEA};
inline constexpr sf::Color Grid{82, 90, 90, 18};
inline constexpr sf::Color Surface{0xFF, 0xFF, 0xFF};
inline constexpr sf::Color InputBackground{0xF2, 0xF4, 0xF4};
inline constexpr sf::Color Text{0x25, 0x29, 0x2B};
inline constexpr sf::Color Muted{0x52, 0x5A, 0x5A};
inline constexpr sf::Color Blue{0x00, 0x5A, 0x94};
inline constexpr sf::Color DarkBlue{0x00, 0x3A, 0x60};
inline constexpr sf::Color LightBlue{0x00, 0xE7, 0xDE};
inline constexpr sf::Color Green{0x00, 0xA5, 0x00};
inline constexpr sf::Color Red{0xCE, 0x00, 0x00};
inline constexpr sf::Color ErrorBackground{0xFF, 0xE8, 0xE3};
inline constexpr sf::Color Hull{0x52, 0x5A, 0x5A};
inline constexpr sf::Color HullLight{0xA5, 0xA5, 0xA5};
inline constexpr sf::Color GameBackground{0x06, 0x2A, 0x45};
inline constexpr sf::Color Overlay{6, 42, 69, 209}; // GameBackground at 82 %
inline constexpr sf::Color Letterbox{0x00, 0x00, 0x00};

/// P1-P4: the colour of each ship. Never alone: each also has a pattern and
/// a "P1"-"P4" tag, so colour-blind players can tell them apart.
inline constexpr std::array<sf::Color, 4> Players{
    sf::Color{0x00, 0xB4, 0xC8}, // Light blue, stripes
    sf::Color{0xC6, 0x21, 0xC6}, // Pink, dots
    sf::Color{0x00, 0xA5, 0x00}, // Green, checks
    sf::Color{0xCE, 0x00, 0x00}, // Red, solid
};

/// The colours of what moves in the arena. `ColourMode::Off` holds the main
/// colour of each sprite, and the other modes repaint it (see `tinted()`).
struct Palette {
    std::array<sf::Color, 4> players;
    std::array<sf::Color, 3> bydo; // Indexed by `bydoPart()`: body, eye, lines
    sf::Color playerMissile;
    sf::Color enemyMissile;
};

/// Indexed by `ColourMode`. Each palette stays distinct in the simulation of
/// that colour blindness (Machado 2009): see tests/client/SettingsTests.cpp.
inline constexpr std::array<Palette, ColourModeCount> Palettes{
    Palette{
        .players = Players,
        .bydo = {sf::Color{0xCE, 0x08, 0x00}, sf::Color{0x00, 0xDE, 0x00}, sf::Color{0x94, 0xDE, 0xDE}},
        .playerMissile = {0x00, 0xC6, 0xFF},
        .enemyMissile = {0xFF, 0xBD, 0x00},
    },
    Palette{
        .players = {sf::Color{0xCA, 0x87, 0x7D},
            sf::Color{0xD9, 0x7E, 0x41},
            sf::Color{0xFD, 0x32, 0xFA},
            sf::Color{0xFC, 0x76, 0xE0}},
        .bydo = {sf::Color{0xF4, 0xFF, 0x92}, sf::Color{0x87, 0x15, 0x03}, sf::Color{0x0B, 0xFF, 0xFE}},
        .playerMissile = {0x8D, 0xB8, 0x15},
        .enemyMissile = {0xD2, 0xFF, 0x28},
    },
    Palette{
        .players = {sf::Color{0x20, 0xA3, 0x3F},
            sf::Color{0xF4, 0x5E, 0x04},
            sf::Color{0x8A, 0x8F, 0x82},
            sf::Color{0x3F, 0x8E, 0xFF}},
        .bydo = {sf::Color{0xE0, 0xF5, 0x19}, sf::Color{0x14, 0x03, 0xD2}, sf::Color{0x5A, 0x03, 0x00}},
        .playerMissile = {0xCE, 0xA9, 0xFF},
        .enemyMissile = {0xE4, 0xE5, 0x89},
    },
    Palette{
        .players = {sf::Color{0xCA, 0x4F, 0xFA},
            sf::Color{0xFF, 0x0C, 0xFD},
            sf::Color{0xA4, 0x94, 0x00},
            sf::Color{0xE8, 0x62, 0x10}},
        .bydo = {sf::Color{0x69, 0xA3, 0xFB}, sf::Color{0x5F, 0x08, 0x00}, sf::Color{0xF4, 0xFF, 0xA5}},
        .playerMissile = {0xF9, 0xA0, 0x07},
        .enemyMissile = {0x01, 0xFF, 0x96},
    },
};

/// `pixel` repainted so that the sprite's main colour `from` becomes `to`: a
/// pixel brighter than `from` fades to white, a darker one to black, so the
/// sprite keeps its shading and details. Greys (the hull) and transparent
/// pixels do not change.
[[nodiscard]]
inline sf::Color tinted(sf::Color pixel, sf::Color from, sf::Color to) noexcept
{
    constexpr int greyChroma = 30; // Up to here it is a hull grey, not a colour
    const auto [low, high] = std::minmax({pixel.r, pixel.g, pixel.b});
    if (pixel.a == 0 || high - low <= greyChroma) {
        return pixel;
    }
    // Perceived brightness: unlike HSL lightness, it tells pure blue from cyan
    const auto luma = [](sf::Color colour) {
        return ((0.299F * static_cast<float>(colour.r)) + (0.587F * static_cast<float>(colour.g))
                   + (0.114F * static_cast<float>(colour.b)))
            / 255.0F;
    };
    const float shade = luma(pixel);
    const float reference = luma(from);
    const auto channel = [&](std::uint8_t value) {
        const auto target = static_cast<float>(value);
        return static_cast<std::uint8_t>(shade < reference
                ? target * shade / reference
                : target + ((255.0F - target) * (shade - reference) / (1.0F - reference)));
    };
    return {channel(to.r), channel(to.g), channel(to.b), pixel.a};
}

/// Which part of the Bydo sprite a pixel belongs to: 0 body (red), 1 eye (green),
/// 2 lines (pale cyan, as much green as blue).
[[nodiscard]]
constexpr std::size_t bydoPart(sf::Color pixel) noexcept
{
    if (pixel.r >= pixel.g && pixel.r >= pixel.b) {
        return 0;
    }
    return pixel.g > pixel.b ? 1 : 2;
}

} // namespace client::theme
