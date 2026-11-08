#pragma once

#include "client/Components/Appearance.hpp"
#include "client/Components/Settings.hpp"
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

/// The images of one colour mode.
struct Skin {
    sf::Texture bydos; // r-typesheet5.gif
    sf::Texture missiles; // r-typesheet1.gif: player and enemy missiles
    sf::Texture pataPatas; // r-typesheet8.gif
    sf::Texture bugs; // r-typesheet16.gif
    sf::Texture cancers; // r-typesheet19.gif
    sf::Texture cruisers; // r-typesheet24.gif
    sf::Texture hoppers; // r-typesheet7.gif
    sf::Texture spinners; // r-typesheet13.gif
    sf::Texture gunners; // r-typesheet14.gif
    sf::Texture mines; // r-typesheet39.gif
    sf::Texture bosses; // r-typesheet30.gif
    sf::Texture forces; // r-typesheet2.gif: the Force
    sf::Texture capsules; // r-typesheet3.gif: the Force capsule
    sf::Texture repairKits; // Drawn at startup (Assets.cpp)
    std::array<sf::Texture, 4> patterns; // P1-P4 swatch patterns (generated)
};

/// Everything loaded from the `assets/` folder, once at startup. Systems keep
/// a reference: it must outlive the game loop.
struct Assets {
    std::array<Skin, ColourModeCount> skins; // Indexed by `ColourMode`
    sf::Texture shipHull; // The ship, the same for everyone
    sf::Texture shipColour; // Its parts drawn on top in the player's colour
    sf::Font display; // Bebas Neue: titles only, it has few accents (names use `bodyBold`)
    sf::Font body; // Atkinson Hyperlegible: readable text, numbers, names and tags
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
/// `mirrored` flips it horizontally (a sprite drawn facing right in its
/// sheet, for an enemy flying left).
struct SpriteFrame {
    const sf::Texture* texture{};
    sf::IntRect rect;
    float scale{1.0F};
    bool mirrored{false};
};

/// The frame of `look` (for the boss, `seconds` since the start picks its
/// animation frame). Ships (`widgets::ship`) and unknown looks have none:
/// unknown ones are drawn as a "?" placeholder.
[[nodiscard]]
std::optional<SpriteFrame> spriteFrame(
    const Assets& assets, ColourMode mode, Look look, float seconds = 0.0F);

/// The `assets/` folder: next to the executable, else in the current folder.
[[nodiscard]]
std::filesystem::path findAssetsFolder(const char* programPath);

} // namespace client
