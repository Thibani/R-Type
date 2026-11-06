// The settings file and the key bindings: no window, files in a temporary folder.

#include "client/Components/Session.hpp"
#include "client/Components/Settings.hpp"
#include "client/Settings.hpp"
#include "client/Singleton.hpp"
#include "client/Systems/SettingsSystem.hpp"
#include "client/Ui/Layout.hpp"
#include "client/Ui/Theme.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

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

TEST(Settings, InterfaceSizeIsReadBack)
{
    Settings settings;
    settings.interfaceSize = 150;

    const std::string text = client::formatSettings(settings);

    EXPECT_NE(text.find("interface = 150"), std::string::npos);
    EXPECT_EQ(client::parseSettings(text).interfaceSize, 150);
}

TEST(Settings, UnknownInterfaceSizeKeepsTheDefault)
{
    for (const char* text : {"interface = 110", "interface = 0", "interface = big", "interface = -75"}) {
        EXPECT_EQ(client::parseSettings(text).interfaceSize, Settings{}.interfaceSize) << text;
    }
}

TEST(Settings, ColourModeIsReadBack)
{
    Settings settings;
    settings.colourMode = client::ColourMode::Tritanopia;

    const std::string text = client::formatSettings(settings);

    EXPECT_NE(text.find("colour = Tritanopia"), std::string::npos);
    EXPECT_EQ(client::parseSettings(text).colourMode, client::ColourMode::Tritanopia);
}

TEST(Settings, BadColourModeKeepsTheDefault)
{
    EXPECT_EQ(client::parseSettings("colour = Purple").colourMode, client::ColourMode::Off);
}

// --- Colour modes -----------------------------------------------------------------

namespace {

using Matrix = std::array<std::array<double, 3>, 3>;
using Lab = std::array<double, 3>;

/// What a dichromat sees, in linear RGB (Machado et al. 2009, severity 1.0).
/// Indexed by `ColourMode` - 1.
constexpr std::array<Matrix, 3> Dichromacy{
    Matrix{{{0.152286, 1.052583, -0.204868}, {0.114503, 0.786281, 0.099216},
        {-0.003882, -0.048116, 1.051998}}},
    Matrix{{{0.367322, 0.860646, -0.227968}, {0.280085, 0.672501, 0.047413},
        {-0.011820, 0.042940, 0.968881}}},
    Matrix{{{1.255528, -0.076749, -0.178779}, {-0.078411, 0.930809, 0.147602},
        {0.004733, 0.691367, 0.303900}}},
};

/// `colour` as a dichromat sees it, in CIE Lab: a distance of 2 is barely visible.
Lab seenAs(sf::Color colour, const Matrix& matrix)
{
    const auto linear = [](std::uint8_t channel) {
        const double value = channel / 255.0;
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    const std::array<double, 3> rgb{linear(colour.r), linear(colour.g), linear(colour.b)};
    std::array<double, 3> seen{};
    for (std::size_t i = 0; i < 3; ++i) {
        const auto& row = matrix.at(i);
        seen.at(i) = std::clamp(
            (row[0] * rgb[0]) + (row[1] * rgb[1]) + (row[2] * rgb[2]), 0.0, 1.0);
    }
    const auto curve = [](double t) {
        return t > 0.008856 ? std::cbrt(t) : (7.787 * t) + (16.0 / 116.0);
    };
    const double x = curve(((0.4124 * seen[0]) + (0.3576 * seen[1]) + (0.1805 * seen[2])) / 0.9505);
    const double y = curve((0.2126 * seen[0]) + (0.7152 * seen[1]) + (0.0722 * seen[2]));
    const double z = curve(((0.0193 * seen[0]) + (0.1192 * seen[1]) + (0.9505 * seen[2])) / 1.089);
    return {(116.0 * y) - 16.0, 500.0 * (x - y), 200.0 * (y - z)};
}

double distance(const Lab& a, const Lab& b)
{
    return std::hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
}

} // namespace

TEST(ColourModes, EveryElementStaysDistinctForThePlayerItIsMadeFor)
{
    constexpr double distinct = 30.0; // Far above "barely visible": told apart at a glance
    for (std::size_t mode = 1; mode < client::ColourModeCount; ++mode) {
        const client::theme::Palette& palette = client::theme::Palettes.at(mode);
        std::vector<sf::Color> colours(palette.players.begin(), palette.players.end());
        colours.insert(colours.end(), {palette.bydo.front(), palette.playerMissile, palette.enemyMissile});

        const Matrix& matrix = Dichromacy.at(mode - 1);
        for (std::size_t a = 0; a < colours.size(); ++a) {
            const Lab seen = seenAs(colours.at(a), matrix);
            EXPECT_GT(distance(seen, seenAs(client::theme::GameBackground, matrix)), distinct)
                << "mode " << mode << ", colour " << a << " on the arena";
            EXPECT_GT(distance(seen, seenAs(sf::Color::White, matrix)), distinct)
                << "mode " << mode << ", colour " << a << " on a white swatch";
            for (std::size_t b = a + 1; b < colours.size(); ++b) {
                EXPECT_GT(distance(seen, seenAs(colours.at(b), matrix)), distinct)
                    << "mode " << mode << ", colours " << a << " and " << b;
            }
        }
    }
}

TEST(ColourModes, TheThreeColoursOfTheBydoStayDistinct)
{
    constexpr double distinct = 30.0;
    for (std::size_t mode = 1; mode < client::ColourModeCount; ++mode) {
        const auto& bydo = client::theme::Palettes.at(mode).bydo;
        const Matrix& matrix = Dichromacy.at(mode - 1);
        const Lab body = seenAs(bydo.at(0), matrix);
        const Lab eye = seenAs(bydo.at(1), matrix);
        const Lab lines = seenAs(bydo.at(2), matrix);
        const Lab arena = seenAs(client::theme::GameBackground, matrix);

        EXPECT_GT(distance(body, eye), distinct) << "mode " << mode;
        EXPECT_GT(distance(body, lines), distinct) << "mode " << mode;
        EXPECT_GT(distance(eye, lines), distinct) << "mode " << mode;
        EXPECT_GT(distance(eye, arena), distinct) << "mode " << mode;
        EXPECT_GT(distance(lines, arena), distinct) << "mode " << mode;
    }
}

TEST(ColourModes, EveryColourOfTheBydoSpriteFindsItsPart)
{
    using client::theme::bydoPart;
    for (const sf::Color body : {sf::Color{0xCE, 0x08, 0x00}, sf::Color{0x6B, 0x08, 0x00},
             sf::Color{0xFF, 0x63, 0x5A}, sf::Color{0x52, 0x00, 0x00}, sf::Color{0xFF, 0xA5, 0x9C}}) {
        EXPECT_EQ(bydoPart(body), 0U);
    }
    for (const sf::Color eye : {sf::Color{0x00, 0x4A, 0x00}, sf::Color{0xC6, 0xFF, 0xC6}, sf::Color{0x00, 0xDE, 0x00}}) {
        EXPECT_EQ(bydoPart(eye), 1U);
    }
    EXPECT_EQ(bydoPart(sf::Color{0x94, 0xDE, 0xDE}), 2U);
}

TEST(ColourModes, TintKeepsGreysAndTransparency)
{
    const sf::Color from{0xCE, 0x00, 0x00};
    const sf::Color to{0x12, 0x34, 0x56};

    EXPECT_EQ(client::theme::tinted(sf::Color{0x73, 0x8C, 0x8C}, from, to), (sf::Color{0x73, 0x8C, 0x8C}));
    EXPECT_EQ(client::theme::tinted(sf::Color::White, from, to), sf::Color::White);
    EXPECT_EQ(client::theme::tinted(sf::Color{255, 132, 66, 0}, from, to), (sf::Color{255, 132, 66, 0}));
}

TEST(ColourModes, TintTurnsTheMainColourIntoThePaletteColour)
{
    const sf::Color from{0xCE, 0x08, 0x00};
    const sf::Color to{0x20, 0x80, 0xC0};

    EXPECT_EQ(client::theme::tinted(from, from, to), to);
}

TEST(ColourModes, TintKeepsTheDetailsOfASprite)
{
    const sf::Color to{0x20, 0x80, 0xC0};
    const auto brightness = [](sf::Color c) { return c.r + c.g + c.b; };
    const auto expectDarkToLight = [&](std::vector<sf::Color> shades, sf::Color from) {
        for (std::size_t i = 1; i < shades.size(); ++i) {
            EXPECT_LT(brightness(client::theme::tinted(shades.at(i - 1), from, to)),
                brightness(client::theme::tinted(shades.at(i), from, to)))
                << "shade " << i;
        }
    };

    // Shades that HSL lightness cannot tell apart: the three blues of the player missile
    expectDarkToLight({{0x00, 0x00, 0xFF}, {0x00, 0x9C, 0xFF}, {0x00, 0xC6, 0xFF}}, {0x00, 0xC6, 0xFF});
    // The enemy missile, around its main colour
    expectDarkToLight({{0xFF, 0xBD, 0x00}, {0xFF, 0xDE, 0x00}, {0xFF, 0xFF, 0x7B}}, {0xFF, 0xBD, 0x00});
    // The body of the Bydo
    expectDarkToLight({{0x52, 0x00, 0x00}, {0x6B, 0x08, 0x00}, {0xCE, 0x08, 0x00}, {0xFF, 0x63, 0x5A}, {0xFF, 0xA5, 0x9C}}, {0xCE, 0x08, 0x00});
}

// --- Interface size ---------------------------------------------------------------

namespace {

bool contains(sf::FloatRect outer, sf::FloatRect inner)
{
    return inner.position.x >= outer.position.x && inner.position.y >= outer.position.y
        && inner.position.x + inner.size.x <= outer.position.x + outer.size.x
        && inner.position.y + inner.size.y <= outer.position.y + outer.size.y;
}

} // namespace

TEST(InterfaceSize, DefaultShowsTheWholeCanvas)
{
    const sf::FloatRect area = client::layout::visibleArea(client::Session{}, 100);

    EXPECT_EQ(area.position, (sf::Vector2f{0.0F, 0.0F}));
    EXPECT_EQ(area.size, (sf::Vector2f{1280.0F, 720.0F}));
}

TEST(InterfaceSize, EverySizeKeepsTheCanvasRatio)
{
    for (const int size : Settings::InterfaceSizes) {
        const sf::FloatRect area = client::layout::visibleArea(client::Session{}, size);
        EXPECT_NEAR(area.size.x / area.size.y, 16.0F / 9.0F, 0.001F) << size;
    }
}

TEST(InterfaceSize, EveryControlStaysVisibleAtEverySize)
{
    using client::Screen;
    using client::Session;
    for (const int size : Settings::InterfaceSizes) {
        for (const Screen screen :
            {Screen::Connect, Screen::Home, Screen::WaitingRoom, Screen::Game, Screen::GameOver}) {
            for (const bool menu : {false, true}) {
                for (const bool settings : {false, true}) {
                    Session session;
                    session.screen = screen;
                    session.menuOpen = menu;
                    session.settingsOpen = settings;
                    const sf::FloatRect area = client::layout::visibleArea(session, size);
                    for (const sf::FloatRect control : client::layout::focusables(session, size)) {
                        EXPECT_TRUE(contains(area, control))
                            << "size " << size << ", screen " << static_cast<int>(screen);
                    }
                }
            }
        }
    }
}

TEST(InterfaceSize, PanelsStayVisibleAtEverySize)
{
    using namespace client::layout;
    for (const int size : Settings::InterfaceSizes) {
        client::Session session;
        EXPECT_TRUE(contains(visibleArea(session, size), ConnectPanel)) << size;
        session.screen = client::Screen::Game;
        EXPECT_TRUE(contains(visibleArea(session, size), MenuPanel)) << size;
        session.settingsOpen = true;
        EXPECT_TRUE(contains(visibleArea(session, size), SettingsPanel)) << size;
    }
}

TEST(InterfaceSize, SettingsRowsNeverOverlapAndStayInThePanel)
{
    using namespace client::layout;
    for (int a = 0; a <= BackRow; ++a) {
        EXPECT_TRUE(contains(SettingsPanel, settingsRow(a))) << a;
        for (int b = a + 1; b <= BackRow; ++b) {
            EXPECT_FALSE(settingsRow(a).findIntersection(settingsRow(b)).has_value()) << a << " " << b;
        }
    }
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

// --- Theme and layout -----------------------------------------------------------

namespace {

/// WCAG contrast ratio: 4.5 is the minimum for normal text.
double contrast(sf::Color first, sf::Color second)
{
    const auto luminance = [](sf::Color colour) {
        const auto linear = [](std::uint8_t channel) {
            const double value = channel / 255.0;
            return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
        };
        return (0.2126 * linear(colour.r)) + (0.7152 * linear(colour.g))
            + (0.0722 * linear(colour.b));
    };
    const double a = luminance(first);
    const double b = luminance(second);
    return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

} // namespace

TEST(Theme, EveryTextIsReadableOnItsBackground)
{
    using namespace client::theme;
    constexpr double readable = 4.5;
    for (const sf::Color background : {Surface, Background, InputBackground}) {
        for (const sf::Color text : {Text, Muted, Blue, Red, Gold}) {
            EXPECT_GE(contrast(text, background), readable);
        }
    }
    EXPECT_GE(contrast(OnPrimary, Primary), readable);
    EXPECT_GE(contrast(Red, ErrorBackground), readable);
}

TEST(Layout, EveryFocusablePartIsInsideTheCanvasAndNoneOverlaps)
{
    using client::Screen;
    const sf::FloatRect canvas{{0.0F, 0.0F}, {client::theme::UiWidth, client::theme::UiHeight}};
    const auto check = [&](const client::Session& session) {
        const auto parts = client::layout::focusables(session, 100);
        for (std::size_t a = 0; a < parts.size(); ++a) {
            EXPECT_TRUE(canvas.contains(parts.at(a).position)
                && canvas.contains(parts.at(a).position + parts.at(a).size))
                << a;
            for (std::size_t b = a + 1; b < parts.size(); ++b) {
                EXPECT_FALSE(parts.at(a).findIntersection(parts.at(b)).has_value())
                    << a << ", " << b;
            }
        }
    };
    for (const Screen screen :
        {Screen::Connect, Screen::Home, Screen::WaitingRoom, Screen::GameOver}) {
        client::Session session;
        session.screen = screen;
        check(session);
    }
    client::Session menu;
    menu.screen = Screen::Game;
    menu.menuOpen = true;
    check(menu);
    client::Session settings;
    settings.settingsOpen = true;
    check(settings);
}
