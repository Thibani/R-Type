// The settings file and the key bindings: no window, files in a temporary folder.

#include "client/Components/Settings.hpp"
#include "client/Settings.hpp"
#include "client/Singleton.hpp"
#include "client/Systems/SettingsSystem.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>

using client::Action;
using client::Settings;
using sf::Keyboard::Key;

namespace {

Key keyOf(const Settings& settings, Action action)
{
    return settings.keys.at(static_cast<std::size_t>(action));
}

/// A fresh folder under the system's temporary directory, removed afterwards.
class TempFolder {
public:
    TempFolder()
        : _path(std::filesystem::temp_directory_path()
              / ("rtype-settings-" + std::to_string(std::random_device{}())))
    {
    }
    ~TempFolder()
    {
        std::error_code ignored;
        std::filesystem::remove_all(_path, ignored);
    }
    TempFolder(const TempFolder&) = delete;
    TempFolder& operator=(const TempFolder&) = delete;
    TempFolder(TempFolder&&) = delete;
    TempFolder& operator=(TempFolder&&) = delete;

    [[nodiscard]]
    const std::filesystem::path& path() const noexcept
    {
        return _path;
    }

private:
    std::filesystem::path _path;
};

} // namespace

// --- Reading and writing --------------------------------------------------------

TEST(Settings, EmptyTextGivesTheDefaults)
{
    const Settings settings = client::parseSettings("");

    EXPECT_EQ(settings.volume, Settings{}.volume);
    EXPECT_EQ(settings.keys, Settings::DefaultKeys);
}

TEST(Settings, WhatIsWrittenIsReadBack)
{
    Settings settings;
    settings.volume = 30;
    ASSERT_TRUE(client::bindKey(settings, Action::Fire, Key::LControl));
    ASSERT_TRUE(client::bindKey(settings, Action::Up, Key::W));

    const Settings read = client::parseSettings(client::formatSettings(settings));

    EXPECT_EQ(read.volume, 30);
    EXPECT_EQ(read.keys, settings.keys);
}

TEST(Settings, FileIsReadableText)
{
    const std::string text = client::formatSettings(Settings{});

    EXPECT_NE(text.find("volume = 80"), std::string::npos);
    EXPECT_NE(text.find("key.fire = Space"), std::string::npos);
}

TEST(Settings, UnknownKeysAndCommentsAreIgnored)
{
    const Settings settings = client::parseSettings(
        "# comment\nfuture.option = 12\n  volume =  40  # trailing comment\nnot a setting\n");

    EXPECT_EQ(settings.volume, 40);
}

TEST(Settings, BadValuesKeepTheirDefaultAndTheRestIsRead)
{
    const Settings settings
        = client::parseSettings("volume = loud\nkey.fire = NoSuchKey\nkey.up = W\n");

    EXPECT_EQ(settings.volume, Settings{}.volume);
    EXPECT_EQ(keyOf(settings, Action::Fire), Key::Space);
    EXPECT_EQ(keyOf(settings, Action::Up), Key::W);
}

TEST(Settings, VolumeIsClamped)
{
    EXPECT_EQ(client::parseSettings("volume = 250").volume, Settings::MaxVolume);
    EXPECT_EQ(client::parseSettings("volume = -5").volume, 0);
}

TEST(Settings, TwoActionsOnOneKeyResetTheControls)
{
    // Only possible by editing the file by hand
    const Settings settings = client::parseSettings("key.up = W\nkey.down = W\n");

    EXPECT_EQ(settings.keys, Settings::DefaultKeys);
}

TEST(Settings, GarbageGivesTheDefaults)
{
    const std::string garbage("\x00\xFF\x10=\n=\x7F==\xC3", 10);

    const Settings settings = client::parseSettings(garbage);

    EXPECT_EQ(settings.volume, Settings{}.volume);
    EXPECT_EQ(settings.keys, Settings::DefaultKeys);
}

// --- Key bindings -----------------------------------------------------------------

TEST(Settings, BindingAKeyChangesThatAction)
{
    Settings settings;

    EXPECT_TRUE(client::bindKey(settings, Action::Fire, Key::X));

    EXPECT_EQ(keyOf(settings, Action::Fire), Key::X);
    EXPECT_EQ(keyOf(settings, Action::Up), Key::Up); // Others untouched
}

TEST(Settings, AKeyAlreadyUsedIsSwapped)
{
    Settings settings; // Up = Up, Fire = Space

    ASSERT_TRUE(client::bindKey(settings, Action::Fire, Key::Up));

    EXPECT_EQ(keyOf(settings, Action::Fire), Key::Up);
    EXPECT_EQ(keyOf(settings, Action::Up), Key::Space); // Never two actions on one key
}

TEST(Settings, EscapeCannotBeBound)
{
    Settings settings;

    EXPECT_FALSE(client::bindKey(settings, Action::Fire, Key::Escape));
    EXPECT_FALSE(client::bindKey(settings, Action::Fire, Key::Unknown));
    EXPECT_EQ(keyOf(settings, Action::Fire), Key::Space);
}

TEST(Settings, EveryBindableKeyHasAStableName)
{
    for (const Key key : {Key::A, Key::Z, Key::Num0, Key::Numpad9, Key::Space, Key::LShift}) {
        const auto name = client::keyName(key);
        ASSERT_TRUE(name.has_value());
        EXPECT_EQ(client::keyFromName(*name), key);
    }
}

// --- Saving -----------------------------------------------------------------------

TEST(Settings, SavedFileIsLoadedBack)
{
    const TempFolder folder;
    const auto path = folder.path() / "nested" / "settings.cfg"; // The folder is created
    Settings settings;
    settings.volume = 0;

    ASSERT_TRUE(client::saveSettings(path, settings).has_value());

    EXPECT_EQ(client::loadSettings(path).volume, 0);
    auto temporary = path;
    temporary += ".tmp";
    EXPECT_FALSE(std::filesystem::exists(temporary)); // Renamed, not left behind
}

TEST(Settings, MissingFileGivesTheDefaults)
{
    const TempFolder folder;

    EXPECT_EQ(client::loadSettings(folder.path() / "none.cfg").volume, Settings{}.volume);
}

TEST(Settings, SystemSavesOnlyWhenChanged)
{
    const TempFolder folder;
    const auto path = folder.path() / "settings.cfg";
    ecs::World world;
    client::SettingsSystem system(path);

    system.update(world, ecs::Time{});
    EXPECT_FALSE(std::filesystem::exists(path)); // Nothing changed

    auto& settings = client::singleton<Settings>(world);
    settings.volume = 20;
    settings.dirty = true;
    system.update(world, ecs::Time{});

    EXPECT_FALSE(settings.dirty);
    EXPECT_TRUE(settings.saveError.empty());
    EXPECT_EQ(client::loadSettings(path).volume, 20);
}

TEST(Settings, WithoutAFolderTheSettingsAreNotSaved)
{
    ecs::World world;
    client::SettingsSystem system(std::nullopt);
    client::singleton<Settings>(world).dirty = true;

    system.update(world, ecs::Time{});

    EXPECT_FALSE(client::singleton<Settings>(world).saveError.empty()); // The player is told
}
