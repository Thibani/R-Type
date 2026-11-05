#include "client/Assets.hpp"

#include "client/Ui/Animation.hpp"
#include "client/Ui/Theme.hpp"

#include <SFML/Graphics/Image.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <system_error>
#include <utility>

namespace client {

namespace {

// Rows of r-typesheet42: light blue, pink, green, red (then blue, unused).
// Column 2 is the ship flying level. 33 × 17 × 2 ≈ the server's 64 × 32 hitbox.
constexpr sf::IntRect shipFrame(int row) noexcept
{
    return {{66, row * 17}, {33, 17}};
}
constexpr sf::IntRect BydoFrame{{5, 6}, {21, 24}};
constexpr sf::IntRect PlayerMissileFrame{{249, 105}, {16, 8}};
constexpr sf::IntRect EnemyMissileFrame{{243, 277}, {12, 10}};

/// The enemy sheets: the same in every colour mode (only players, Bydos and
/// missiles are recoloured).
struct EnemySheet {
    sf::Texture Skin::*texture;
    const char* file;
};
constexpr std::array<EnemySheet, 9> EnemySheets{
    EnemySheet{.texture = &Skin::pataPatas, .file = "r-typesheet8.gif"},
    EnemySheet{.texture = &Skin::bugs, .file = "r-typesheet16.gif"},
    EnemySheet{.texture = &Skin::cancers, .file = "r-typesheet19.gif"},
    EnemySheet{.texture = &Skin::cruisers, .file = "r-typesheet24.gif"},
    EnemySheet{.texture = &Skin::hoppers, .file = "r-typesheet7.gif"},
    EnemySheet{.texture = &Skin::spinners, .file = "r-typesheet13.gif"},
    EnemySheet{.texture = &Skin::gunners, .file = "r-typesheet14.gif"},
    EnemySheet{.texture = &Skin::mines, .file = "r-typesheet39.gif"},
    EnemySheet{.texture = &Skin::bosses, .file = "r-typesheet30.gif"}};

/// A 12 × 12 tile of player `index`'s swatch pattern: stripes, dots, checks
/// or solid colour (from the mockup's style guide).
sf::Image patternTile(std::size_t index, sf::Color colour)
{
    constexpr unsigned Size = 12;
    sf::Image image({Size, Size}, sf::Color::White);
    for (unsigned y = 0; y < Size; ++y) {
        for (unsigned x = 0; x < Size; ++x) {
            bool filled = true; // P4: solid
            if (index == 0) {
                filled = (x + y) % Size < Size / 2; // Diagonal stripes
            } else if (index == 1) {
                const int dx = static_cast<int>(x % 6) - 2;
                const int dy = static_cast<int>(y % 6) - 2;
                filled = dx * dx + dy * dy <= 3; // Dots
            } else if (index == 2) {
                filled = (x < Size / 2) == (y < Size / 2); // Checks
            }
            if (filled) {
                image.setPixel({x, y}, colour);
            }
        }
    }
    return image;
}

template <typename Paint>
void repaint(sf::Image& image, sf::IntRect frame, Paint paint)
{
    for (int y = frame.position.y; y < frame.position.y + frame.size.y; ++y) {
        for (int x = frame.position.x; x < frame.position.x + frame.size.x; ++x) {
            const sf::Vector2u at(sf::Vector2i{x, y});
            image.setPixel(at, paint(image.getPixel(at)));
        }
    }
}

bool loadTexture(sf::Texture& texture, const sf::Image& image)
{
    if (!texture.loadFromImage(image)) {
        return false;
    }
    texture.setSmooth(false); // Pixel art stays sharp when enlarged
    return true;
}

void tintFrame(sf::Image& image, sf::IntRect frame, sf::Color from, sf::Color to)
{
    repaint(image, frame, [&](sf::Color pixel) { return theme::tinted(pixel, from, to); });
}

/// The skin of `mode`, from the original sprite sheets.
bool loadSkin(Skin& skin, ColourMode mode, sf::Image ships, sf::Image bydos, sf::Image missiles,
    const std::array<sf::Image, EnemySheets.size()>& enemies)
{
    const theme::Palette& palette = theme::Palettes.at(static_cast<std::size_t>(mode));
    if (mode != ColourMode::Off) {
        const theme::Palette& original = theme::Palettes.at(static_cast<std::size_t>(ColourMode::Off));
        for (std::size_t row = 0; row < palette.players.size(); ++row) {
            tintFrame(ships, shipFrame(static_cast<int>(row)), original.players.at(row), palette.players.at(row));
        }
        repaint(bydos, BydoFrame, [&](sf::Color pixel) {
            const std::size_t part = theme::bydoPart(pixel);
            return theme::tinted(pixel, original.bydo.at(part), palette.bydo.at(part));
        });
        tintFrame(missiles, PlayerMissileFrame, original.playerMissile, palette.playerMissile);
        tintFrame(missiles, EnemyMissileFrame, original.enemyMissile, palette.enemyMissile);
    }
    for (std::size_t i = 0; i < skin.patterns.size(); ++i) {
        if (!skin.patterns.at(i).loadFromImage(patternTile(i, palette.players.at(i)))) {
            return false;
        }
        skin.patterns.at(i).setRepeated(true);
    }
    for (std::size_t i = 0; i < EnemySheets.size(); ++i) {
        if (!loadTexture(skin.*EnemySheets.at(i).texture, enemies.at(i))) {
            return false;
        }
    }
    return loadTexture(skin.ships, ships) && loadTexture(skin.bydos, bydos)
        && loadTexture(skin.missiles, missiles);
}

} // namespace

std::expected<void, std::string> Assets::load(Assets& assets, const std::filesystem::path& folder)
{
    const auto fail = [](const std::filesystem::path& path) {
        return std::unexpected("cannot load '" + path.string() + "'");
    };

    const std::filesystem::path sprites = folder / "sprites";
    sf::Image ships;
    sf::Image bydos;
    sf::Image missiles;
    for (const auto& [image, file] : {std::pair{&ships, "r-typesheet42.gif"},
             std::pair{&bydos, "r-typesheet5.gif"},
             std::pair{&missiles, "r-typesheet1.gif"}}) {
        if (!image->loadFromFile(sprites / file)) {
            return fail(sprites / file);
        }
    }
    std::array<sf::Image, EnemySheets.size()> enemies;
    for (std::size_t i = 0; i < EnemySheets.size(); ++i) {
        if (!enemies.at(i).loadFromFile(sprites / EnemySheets.at(i).file)) {
            return fail(sprites / EnemySheets.at(i).file);
        }
    }
    for (std::size_t mode = 0; mode < assets.skins.size(); ++mode) {
        if (!loadSkin(assets.skins.at(mode), static_cast<ColourMode>(mode), ships, bydos, missiles,
                enemies)) {
            return std::unexpected("cannot create the textures of the colour modes");
        }
    }

    const std::filesystem::path fonts = folder / "fonts";
    for (const auto& [font, file] : {std::pair{&assets.display, "PixelifySans.ttf"},
             std::pair{&assets.body, "AtkinsonHyperlegible-Regular.ttf"},
             std::pair{&assets.bodyBold, "AtkinsonHyperlegible-Bold.ttf"}}) {
        if (!font->openFromFile(fonts / file)) {
            return fail(fonts / file);
        }
    }

    const std::filesystem::path sounds = folder / "sounds";
    const std::array<const char*, 4> soundFiles{
        "shot.wav", "explosion.wav", "player_down.wav", "player_joined.wav"};
    for (std::size_t i = 0; i < soundFiles.size(); ++i) {
        if (!assets.sounds.at(i).loadFromFile(sounds / soundFiles.at(i))) {
            return fail(sounds / soundFiles.at(i));
        }
    }
    return {};
}

const sf::SoundBuffer& Assets::sound(Sound sound) const
{
    return sounds.at(static_cast<std::size_t>(sound));
}

namespace {

/// The `size` rectangle at `corner` in `texture`, enlarged `scale` times.
SpriteFrame frame(const sf::Texture& texture, sf::Vector2i corner, sf::Vector2i size, float scale,
    bool mirrored = false)
{
    return SpriteFrame{.texture = &texture,
        .rect = sf::IntRect(corner, size),
        .scale = scale,
        .mirrored = mirrored};
}

/// The boss (r-typesheet30): rows 1 to 8 of the sheet, 4 frames each, show its
/// belly opening and the creature inside coming out. Row 0 is left out: its
/// belly is an empty hole, meant to be filled by separate small sprites.
SpriteFrame bossFrame(const Skin& skin, float seconds)
{
    constexpr std::array<int, 8> rowTops{217, 430, 644, 861, 1073, 1285, 1498, 1711};
    constexpr int columns = 4;
    constexpr float framesPerSecond = 10.0F;
    const std::size_t index = pingPongFrame(rowTops.size() * columns, framesPerSecond, seconds);
    const int row = static_cast<int>(index) / columns;
    const int column = static_cast<int>(index) % columns;
    // 155 × 204 cells, 162 units apart. × 2: hitbox 240 × 360 (its body, not the tail tips)
    return frame(skin.bosses, {27 + (column * 162), rowTops.at(static_cast<std::size_t>(row))},
        {155, 204}, 2.0F);
}

} // namespace

std::optional<SpriteFrame> spriteFrame(
    const Assets& assets, ColourMode mode, Look look, std::uint8_t player, float seconds)
{
    const Skin& skin = assets.skins.at(static_cast<std::size_t>(mode));
    switch (look) {
    case Look::PlayerShip: {
        const int row = player < theme::Players.size() ? player : 0;
        return SpriteFrame{.texture = &skin.ships, .rect = shipFrame(row), .scale = 2.0F};
    }
    case Look::Bydo:
        return SpriteFrame{.texture = &skin.bydos, .rect = BydoFrame, .scale = 2.5F};
    case Look::PlayerMissile:
        return SpriteFrame{.texture = &skin.missiles, .rect = PlayerMissileFrame, .scale = 2.0F};
    case Look::EnemyMissile:
        return SpriteFrame{.texture = &skin.missiles, .rect = EnemyMissileFrame, .scale = 2.0F};
    // Enemies: the sprite is about the size of the server's hitbox (game/Enemies.hpp)
    case Look::PataPata: // 22 × 22 × 2 = 44: hitbox 40 × 40
        return frame(skin.pataPatas, {7, 7}, {22, 22}, 2.0F);
    case Look::Bug: // Head first, flying left. 16 × 9 × 3: hitbox 44 × 24
        return frame(skin.bugs, {324, 12}, {16, 9}, 3.0F);
    case Look::Cancer: // Facing the player. 24 × 21 × 2.5: hitbox 56 × 48
        return frame(skin.cancers, {103, 6}, {24, 21}, 2.5F);
    case Look::Cruiser: // Drawn facing right in the sheet. 48 × 48 × 2.5: hitbox 108 × 108
        return frame(skin.cruisers, {9, 9}, {48, 48}, 2.5F, true);
    case Look::Hopper: // Standing. 32 × 29 × 2.5: hitbox 80 × 80
        return frame(skin.hoppers, {1, 4}, {32, 29}, 2.5F);
    case Look::Spinner: // 32 × 29 × 2.5: hitbox 80 × 72
        return frame(skin.spinners, {1, 3}, {32, 29}, 2.5F);
    case Look::Gunner: // Gun forward. 48 × 47 × 2: hitbox 88 × 88
        return frame(skin.gunners, {1, 2}, {48, 47}, 2.0F);
    case Look::Mine: // 64 × 64 × 1.7: hitbox 104 × 104
        return frame(skin.mines, {34, 2}, {64, 64}, 1.7F);
    case Look::Boss: // Animated
        return bossFrame(skin, seconds);
    case Look::Unknown:
        break;
    }
    return std::nullopt;
}

std::filesystem::path findAssetsFolder(const char* programPath)
{
    std::error_code error;
    const auto besideProgram
        = std::filesystem::absolute(programPath, error).parent_path() / "assets";
    if (!error && std::filesystem::is_directory(besideProgram, error)) {
        return besideProgram;
    }
    return "assets";
}

} // namespace client
