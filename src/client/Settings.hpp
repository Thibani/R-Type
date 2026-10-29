#pragma once

#include "client/Components/Settings.hpp"

#include <SFML/Window/Keyboard.hpp>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

/// The settings file: plain `key = value` lines, read leniently.
///
///     # R-Type client settings
///     volume = 80
///     key.up = Up
///     key.fire = Space
///
/// Unknown keys are ignored (older and newer clients can share a file); a bad
/// value keeps its default; a missing or unreadable file gives the defaults.
/// Reading never fails, so a broken file can never stop the client.
namespace client {

/// Stable name of a bindable key in the file ("A", "Num5", "Space", "LShift"),
/// or nullopt for a key that cannot be bound (Escape is kept for menus).
[[nodiscard]]
std::optional<std::string_view> keyName(sf::Keyboard::Key key) noexcept;

/// The key named `name` in the file, if it is a bindable key.
[[nodiscard]]
std::optional<sf::Keyboard::Key> keyFromName(std::string_view name) noexcept;

/// Binds `key` to `action`. A key already used by another action is swapped
/// with it, so two actions never share a key. False if the key is not bindable.
bool bindKey(Settings& settings, Action action, sf::Keyboard::Key key) noexcept;

[[nodiscard]]
Settings parseSettings(std::string_view text);

[[nodiscard]]
std::string formatSettings(const Settings& settings);

/// Where the settings live: `%APPDATA%\R-Type\settings.cfg` on Windows,
/// `$XDG_CONFIG_HOME/r-type/settings.cfg` (else `~/.config/r-type/`) elsewhere.
/// Nullopt when no such folder is known: settings are then not saved.
[[nodiscard]]
std::optional<std::filesystem::path> settingsPath();

/// The settings in `path`, or the defaults if it cannot be read.
[[nodiscard]]
Settings loadSettings(const std::filesystem::path& path);

/// Writes the settings to `path`, creating its folder. Written to a temporary
/// file first, then renamed: a crash never leaves a half-written file.
[[nodiscard]]
std::expected<void, std::string> saveSettings(
    const std::filesystem::path& path, const Settings& settings);

} // namespace client
