#pragma once

#include "client/Components/Settings.hpp"

#include <SFML/Graphics/Color.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <utility>
#include <cstddef>
#include <cstdint>

/// The dark style of the Figma mockup: bevelled panels on a starry background.
/// Each text colour stays readable on its background: see tests/client/SettingsTests.cpp.
namespace client::theme {

// The interface is laid out on a 1280 × 720 canvas, the game on the
// 1920 × 1080 arena: both are scaled to the window, keeping 16:9.
inline constexpr float UiWidth = 1280.0F;
inline constexpr float UiHeight = 720.0F;

inline constexpr sf::Color Background{0x0A, 0x0E, 0x14};
inline constexpr sf::Color Surface{0x11, 0x1C, 0x27};
inline constexpr sf::Color InputBackground{0x0A, 0x0E, 0x14};
inline constexpr sf::Color Text{0xF1, 0xF5, 0xF7};
inline constexpr sf::Color Muted{0x93, 0xA7, 0xB6};
// The accent: focus, caret, status. Still called Blue for the branches using it
inline constexpr sf::Color Blue{0x2F, 0xE6, 0xD0};
inline constexpr sf::Color LightBlue = Blue;
inline constexpr sf::Color Primary{0xFF, 0x5A, 0x36}; // Main button
inline constexpr sf::Color OnPrimary{0x0A, 0x0E, 0x14}; // Text on the main button
inline constexpr sf::Color Gold{0xC9, 0xA4, 0x5C}; // Decoration
inline constexpr sf::Color Streak{0xC9, 0xA4, 0x5C, 70}; // Gold lines crossing the background
inline constexpr sf::Color Green{0x38, 0xD2, 0x7A};
inline constexpr sf::Color Red{0xFF, 0x5A, 0x36};
inline constexpr sf::Color ErrorBackground{0x2A, 0x14, 0x10};
inline constexpr sf::Color Hull{0x30, 0x41, 0x4F};
inline constexpr sf::Color ChatFrame{0xF1, 0xF5, 0xF7, 10}; // Pale: the game stays visible
inline constexpr sf::Color ChatFrameOpen{0x0A, 0x0E, 0x14, 170}; // While typing
inline constexpr sf::Color ChatBorder{0xF1, 0xF5, 0xF7, 36};
inline constexpr sf::Color ChatOutline{0xF1, 0xF5, 0xF7, 170}; // Around the game chat: no fill
inline constexpr sf::Color GameBackground = Background;
inline constexpr sf::Color Overlay{10, 14, 20, 209}; // GameBackground at 82 %
inline constexpr sf::Color Letterbox{0x00, 0x00, 0x00};

/// A ship in the arena, close to its hitbox: units, as drawn everywhere in these proportions.
inline constexpr float ShipWidth = 72.0F;
inline constexpr float ShipHeight = 26.0F;

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

/// True for a pixel `tinted()` repaints: visible, and not a hull grey.
[[nodiscard]]
inline bool isTintable(sf::Color pixel) noexcept
{
    constexpr int greyChroma = 30; // Up to here it is a hull grey, not a colour
    const auto [low, high] = std::minmax({pixel.r, pixel.g, pixel.b});
    return pixel.a != 0 && high - low > greyChroma;
}

/// Perceived brightness, from 0 to 1: unlike HSL lightness, it tells pure blue
/// from cyan.
[[nodiscard]]
constexpr float luma(sf::Color colour) noexcept
{
    return ((0.299F * static_cast<float>(colour.r)) + (0.587F * static_cast<float>(colour.g))
               + (0.114F * static_cast<float>(colour.b)))
        / 255.0F;
}

/// A grey as bright as the average tintable pixel, given each colour of a
/// sheet and how many pixels have it: the `from` of `tinted()` for a sheet with
/// no single main colour, so its middle shades get the new colour. Mid grey if
/// none is tintable.
[[nodiscard]]
inline sf::Color averageShade(std::span<const std::pair<sf::Color, std::size_t>> colours) noexcept
{
    double total = 0.0;
    std::size_t count = 0;
    for (const auto& [colour, pixels] : colours) {
        if (isTintable(colour)) {
            total += static_cast<double>(luma(colour)) * static_cast<double>(pixels);
            count += pixels;
        }
    }
    const double shade = count == 0 ? 0.5 : total / static_cast<double>(count);
    const auto grey = static_cast<std::uint8_t>(shade * 255.0);
    return {grey, grey, grey};
}

/// `pixel` repainted so that the sprite's main colour `from` becomes `to`: a
/// pixel brighter than `from` fades to white, a darker one to black, so the
/// sprite keeps its shading and details. Greys (the hull) and transparent
/// pixels do not change.
[[nodiscard]]
inline sf::Color tinted(sf::Color pixel, sf::Color from, sf::Color to) noexcept
{
    if (!isTintable(pixel)) {
        return pixel;
    }
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
