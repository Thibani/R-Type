#include "client/Settings.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <system_error>
#include <utility>

namespace client {

namespace {

using sf::Keyboard::Key;
using NamedKey = std::pair<Key, std::string_view>;

/// Every key that can be bound, with its name in the file. Escape is left out
/// on purpose: it opens and closes the menus.
constexpr auto BindableKeys = std::to_array<NamedKey>({
    {Key::A, "A"}, {Key::B, "B"}, {Key::C, "C"}, {Key::D, "D"},
    {Key::E, "E"}, {Key::F, "F"}, {Key::G, "G"}, {Key::H, "H"},
    {Key::I, "I"}, {Key::J, "J"}, {Key::K, "K"}, {Key::L, "L"},
    {Key::M, "M"}, {Key::N, "N"}, {Key::O, "O"}, {Key::P, "P"},
    {Key::Q, "Q"}, {Key::R, "R"}, {Key::S, "S"}, {Key::T, "T"},
    {Key::U, "U"}, {Key::V, "V"}, {Key::W, "W"}, {Key::X, "X"},
    {Key::Y, "Y"}, {Key::Z, "Z"},
    {Key::Num0, "Num0"}, {Key::Num1, "Num1"}, {Key::Num2, "Num2"},
    {Key::Num3, "Num3"}, {Key::Num4, "Num4"}, {Key::Num5, "Num5"},
    {Key::Num6, "Num6"}, {Key::Num7, "Num7"}, {Key::Num8, "Num8"},
    {Key::Num9, "Num9"},
    {Key::Numpad0, "Numpad0"}, {Key::Numpad1, "Numpad1"},
    {Key::Numpad2, "Numpad2"}, {Key::Numpad3, "Numpad3"},
    {Key::Numpad4, "Numpad4"}, {Key::Numpad5, "Numpad5"},
    {Key::Numpad6, "Numpad6"}, {Key::Numpad7, "Numpad7"},
    {Key::Numpad8, "Numpad8"}, {Key::Numpad9, "Numpad9"},
    {Key::Up, "Up"}, {Key::Down, "Down"}, {Key::Left, "Left"},
    {Key::Right, "Right"}, {Key::Space, "Space"}, {Key::Enter, "Enter"},
    {Key::Tab, "Tab"}, {Key::Backspace, "Backspace"},
    {Key::LShift, "LShift"}, {Key::RShift, "RShift"},
    {Key::LControl, "LControl"}, {Key::RControl, "RControl"},
    {Key::LAlt, "LAlt"}, {Key::RAlt, "RAlt"},
    {Key::Insert, "Insert"}, {Key::Delete, "Delete"},
    {Key::Home, "Home"}, {Key::End, "End"},
    {Key::PageUp, "PageUp"}, {Key::PageDown, "PageDown"},
    {Key::Comma, "Comma"}, {Key::Period, "Period"},
    {Key::Semicolon, "Semicolon"}, {Key::Slash, "Slash"},
});

/// File names of the actions, in `Action` order.
constexpr std::array<std::string_view, ActionCount> ActionKeys{
    "key.up", "key.down", "key.left", "key.right", "key.fire",
};

/// A settings file is a few hundred bytes: anything much bigger is not one.
constexpr std::size_t MaxFileSize = std::size_t{64} * 1024;

std::string_view trim(std::string_view text) noexcept
{
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

bool sharesAKey(const Settings& settings) noexcept
{
    for (std::size_t a = 0; a < ActionCount; ++a) {
        for (std::size_t b = a + 1; b < ActionCount; ++b) {
            if (settings.keys.at(a) == settings.keys.at(b)) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

std::optional<std::string_view> keyName(sf::Keyboard::Key key) noexcept
{
    const auto* found = std::ranges::find(BindableKeys, key, &NamedKey::first);
    if (found == BindableKeys.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::optional<sf::Keyboard::Key> keyFromName(std::string_view name) noexcept
{
    const auto* found = std::ranges::find(BindableKeys, name, &NamedKey::second);
    if (found == BindableKeys.end()) {
        return std::nullopt;
    }
    return found->first;
}

bool bindKey(Settings& settings, Action action, sf::Keyboard::Key key) noexcept
{
    if (!keyName(key)) {
        return false;
    }
    auto& keys = settings.keys;
    const auto index = static_cast<std::size_t>(action);
    // The action that had this key gets ours: two actions never share a key
    if (auto* other = std::ranges::find(keys, key); other != keys.end()) {
        *other = keys.at(index);
    }
    keys.at(index) = key;
    return true;
}

Settings parseSettings(std::string_view text)
{
    Settings settings;
    while (!text.empty()) {
        const auto end = text.find('\n');
        std::string_view line = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);

        line = trim(line.substr(0, line.find('#')));
        const auto equal = line.find('=');
        if (equal == std::string_view::npos) {
            continue; // Blank, comment or not a setting
        }
        const std::string_view key = trim(line.substr(0, equal));
        const std::string_view value = trim(line.substr(equal + 1));

        if (key == "volume") {
            int volume = 0;
            const char* last = std::to_address(value.end());
            const auto [rest, error] = std::from_chars(std::to_address(value.begin()), last, volume);
            if (error == std::errc{} && rest == last) {
                settings.volume = std::clamp(volume, 0, Settings::MaxVolume);
            }
        } else {
            for (std::size_t action = 0; action < ActionCount; ++action) {
                const auto bound = keyFromName(value);
                if (ActionKeys.at(action) == key && bound) {
                    settings.keys.at(action) = *bound;
                }
            }
        }
    }
    if (sharesAKey(settings)) {
        settings.keys = Settings::DefaultKeys; // Edited by hand into a conflict
    }
    return settings;
}

std::string formatSettings(const Settings& settings)
{
    std::string text = "# R-Type client settings\n";
    text += "volume = " + std::to_string(settings.volume) + '\n';
    for (std::size_t action = 0; action < ActionCount; ++action) {
        text += std::string(ActionKeys.at(action)) + " = "
            + std::string(keyName(settings.keys.at(action)).value_or("Space")) + '\n';
    }
    return text;
}

std::optional<std::filesystem::path> settingsPath()
{
    const auto variable = [](const char* name) -> std::string {
#ifdef _MSC_VER
#pragma warning(suppress : 4996) // Read once at start, never written: getenv is fine
#endif
        const char* value = std::getenv(name); // NOLINT(concurrency-mt-unsafe)
        return value != nullptr ? value : "";
    };
#ifdef _WIN32
    if (const std::string appData = variable("APPDATA"); !appData.empty()) {
        return std::filesystem::path(appData) / "R-Type" / "settings.cfg";
    }
#else
    if (const std::string config = variable("XDG_CONFIG_HOME"); !config.empty()) {
        return std::filesystem::path(config) / "r-type" / "settings.cfg";
    }
    if (const std::string home = variable("HOME"); !home.empty()) {
        return std::filesystem::path(home) / ".config" / "r-type" / "settings.cfg";
    }
#endif
    return std::nullopt;
}

Settings loadSettings(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return Settings{}; // First start, or unreadable: the defaults
    }
    std::string text(MaxFileSize, '\0');
    file.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<std::size_t>(file.gcount()));
    return parseSettings(text);
}

std::expected<void, std::string> saveSettings(
    const std::filesystem::path& path, const Settings& settings)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return std::unexpected("cannot create " + path.parent_path().string() + ": " + error.message());
    }
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file << formatSettings(settings);
        if (!file.flush()) {
            return std::unexpected("cannot write " + temporary.string());
        }
    }
    std::filesystem::rename(temporary, path, error); // Replaces the old file in one step
    if (error) {
        std::filesystem::remove(temporary, error);
        return std::unexpected("cannot replace " + path.string());
    }
    return {};
}

} // namespace client
