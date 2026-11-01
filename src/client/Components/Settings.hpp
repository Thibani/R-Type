#pragma once

#include <SFML/Window/Keyboard.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace client {

/// The game actions a player can bind to a key (the INPUT mask bits, §4.2).
/// Menus keep fixed keys (arrows, Tab, Enter, Esc), so nobody can lock
/// themselves out of the settings.
enum class Action : std::uint8_t {
    Up,
    Down,
    Left,
    Right,
    Fire,
};
inline constexpr std::size_t ActionCount = 5;

/// Singleton: the player's settings, loaded at start and saved to a file
/// (`Settings.hpp`) whenever they change.
struct Settings {
    static constexpr int MaxVolume = 100;
    static constexpr int VolumeStep = 10;
    static constexpr std::array<sf::Keyboard::Key, ActionCount> DefaultKeys{
        sf::Keyboard::Key::Up,
        sf::Keyboard::Key::Down,
        sf::Keyboard::Key::Left,
        sf::Keyboard::Key::Right,
        sf::Keyboard::Key::Space,
    };

    int volume{80}; // Sound effects, 0 (muted) to 100 %
    std::array<sf::Keyboard::Key, ActionCount> keys{DefaultKeys}; // Indexed by Action

    bool dirty{false}; // Changed since the last save: SettingsSystem saves it
    std::string saveError; // Why the last save failed, shown on the settings screen
};

} // namespace client
