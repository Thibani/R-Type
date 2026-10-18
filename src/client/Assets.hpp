#pragma once

#include "client/Components/Appearance.hpp"
#include "client/Components/SoundEvent.hpp"

#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace client {

/// Everything loaded from the `assets/` folder, once at startup. Systems keep
/// a reference: it must outlive the game loop.
struct Assets {
    sf::Texture ships; // r-typesheet42.gif: one row per ship colour
    sf::Texture bydos; // r-typesheet5.gif
    sf::Texture missiles; // r-typesheet1.gif: player and enemy missiles
    std::array<sf::Texture, 4> patterns; // P1-P4 swatch patterns (generated)
    sf::Font display; // Pixelify Sans: titles, tags, buttons
    sf::Font body; // Atkinson Hyperlegible: readable text
    sf::Font bodyBold;
    std::array<sf::SoundBuffer, 4> sounds; // Indexed by `Sound`

    /// Loads every file from `folder`. Returns the path that failed.
    [[nodiscard]]
    static std::expected<void, std::string> load(
        Assets& assets, const std::filesystem::path& folder);

    [[nodiscard]]
    const sf::SoundBuffer& sound(Sound sound) const;
};

/// Where an entity's image is, and how much it is enlarged in the arena.
struct SpriteFrame {
    const sf::Texture* texture;
    sf::IntRect rect;
    float scale;
};

/// The frame of `look` (for a ship, `player` selects the colour row).
/// Unknown looks have no frame: they are drawn as a "?" placeholder.
[[nodiscard]]
std::optional<SpriteFrame> spriteFrame(const Assets& assets, Look look, std::uint8_t player);

/// The `assets/` folder: next to the executable, else in the current folder.
[[nodiscard]]
std::filesystem::path findAssetsFolder(const char* programPath);

} // namespace client
