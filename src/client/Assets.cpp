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

constexpr sf::IntRect BydoFrame{{5, 6}, {21, 24}};
constexpr sf::IntRect PlayerMissileFrame{{249, 105}, {16, 8}};
constexpr sf::IntRect EnemyMissileFrame{{243, 277}, {12, 10}};

/// The sheets that are the same in every colour mode (only players, Bydos
/// and missiles are recoloured): enemies and gadgets.
struct SharedSheet {
    sf::Texture Skin::* texture;
    const char* file;
};
constexpr std::array<SharedSheet, 11> SharedSheets{
    SharedSheet{.texture = &Skin::pataPatas, .file = "r-typesheet8.gif"},
    SharedSheet{.texture = &Skin::bugs, .file = "r-typesheet16.gif"},
    SharedSheet{.texture = &Skin::cancers, .file = "r-typesheet19.gif"},
    SharedSheet{.texture = &Skin::cruisers, .file = "r-typesheet24.gif"},
    SharedSheet{.texture = &Skin::hoppers, .file = "r-typesheet7.gif"},
    SharedSheet{.texture = &Skin::spinners, .file = "r-typesheet13.gif"},
    SharedSheet{.texture = &Skin::gunners, .file = "r-typesheet14.gif"},
    SharedSheet{.texture = &Skin::mines, .file = "r-typesheet39.gif"},
    SharedSheet{.texture = &Skin::bosses, .file = "r-typesheet30.gif"},
    SharedSheet{.texture = &Skin::forces, .file = "r-typesheet2.gif"},
    SharedSheet{.texture = &Skin::capsules, .file = "r-typesheet3.gif"}};

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

/// The repair kit, drawn here (no sheet has one): a white case with a red
/// cross, 16 × 16. The cross is told apart by its shape in every colour mode.
sf::Image repairKitImage()
{
    constexpr unsigned size = 16;
    sf::Image image({size, size}, sf::Color::Transparent);
    for (unsigned y = 1; y + 1 < size; ++y) {
        for (unsigned x = 1; x + 1 < size; ++x) {
            const bool edge = x == 1 || y == 1 || x == size - 2 || y == size - 2;
            const bool cross = (x >= 6 && x <= 9 && y >= 3 && y <= 12)
                || (y >= 6 && y <= 9 && x >= 3 && x <= 12);
            sf::Color colour = sf::Color::White;
            if (edge) {
                colour = theme::Hull;
            } else if (cross) {
                colour = theme::Red;
            }
            image.setPixel({x, y}, colour);
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
bool loadSkin(Skin& skin, ColourMode mode, sf::Image bydos, sf::Image missiles,
    const std::array<sf::Image, SharedSheets.size()>& shared)
{
    const theme::Palette& palette = theme::Palettes.at(static_cast<std::size_t>(mode));
    if (mode != ColourMode::Off) {
        const theme::Palette& original
            = theme::Palettes.at(static_cast<std::size_t>(ColourMode::Off));
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
    for (std::size_t i = 0; i < SharedSheets.size(); ++i) {
        if (!loadTexture(skin.*SharedSheets.at(i).texture, shared.at(i))) {
            return false;
        }
    }
    return loadTexture(skin.repairKits, repairKitImage()) && loadTexture(skin.bydos, bydos)
        && loadTexture(skin.missiles, missiles);
}

} // namespace

std::expected<void, std::string> Assets::load(Assets& assets, const std::filesystem::path& folder)
{
    const auto fail = [](const std::filesystem::path& path) {
        return std::unexpected("cannot load '" + path.string() + "'");
    };

    const std::filesystem::path sprites = folder / "sprites";
    sf::Image bydos;
    sf::Image missiles;
    for (const auto& [image, file] :
        {std::pair{&bydos, "r-typesheet5.gif"}, std::pair{&missiles, "r-typesheet1.gif"}}) {
        if (!image->loadFromFile(sprites / file)) {
            return fail(sprites / file);
        }
    }
    std::array<sf::Image, SharedSheets.size()> shared;
    for (std::size_t i = 0; i < SharedSheets.size(); ++i) {
        if (!shared.at(i).loadFromFile(sprites / SharedSheets.at(i).file)) {
            return fail(sprites / SharedSheets.at(i).file);
        }
    }
    for (std::size_t mode = 0; mode < assets.skins.size(); ++mode) {
        if (!loadSkin(
                assets.skins.at(mode), static_cast<ColourMode>(mode), bydos, missiles, shared)) {
            return std::unexpected("cannot create the textures of the colour modes");
        }
    }

    const std::filesystem::path ship = folder / "ship";
    for (const auto& [texture, file] :
        {std::pair{&assets.shipHull, "hull.png"}, std::pair{&assets.shipColour, "colour.png"}}) {
        if (!texture->loadFromFile(ship / file) || !texture->generateMipmap()) {
            return fail(ship / file);
        }
        texture->setSmooth(true); // A large drawing, shown small: not pixel art
    }

    const std::filesystem::path fonts = folder / "fonts";
    for (const auto& [font, file] : {std::pair{&assets.display, "BebasNeue-Regular.ttf"},
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

/// The Force (r-typesheet2, second row): 12 frames of the pod turning round.
SpriteFrame forceFrame(const Skin& skin, float seconds)
{
    constexpr std::array<int, 12> lefts{159, 183, 207, 231, 255, 279, 301, 325, 349, 373, 397, 421};
    const std::size_t index = loopFrame(lefts.size(), 15.0F, seconds);
    return frame(skin.forces, {lefts.at(index), 35}, {20, 16}, 2.5F);
}

} // namespace

std::optional<SpriteFrame> spriteFrame(
    const Assets& assets, ColourMode mode, Look look, float seconds)
{
    const Skin& skin = assets.skins.at(static_cast<std::size_t>(mode));
    switch (look) {
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
    // Gadgets (game/Gadgets.hpp)
    case Look::Force: // Turning round. 20 × 16 × 2.5: hitbox 50 × 40
        return forceFrame(skin, seconds);
    case Look::ForceCapsule: { // Opening and closing. 16 × 16 × 2.5: hitbox 40 × 40
        const auto index = static_cast<int>(pingPongFrame(12, 12.0F, seconds));
        return frame(skin.capsules, {1 + (index * 17), 1}, {16, 16}, 2.5F);
    }
    case Look::RepairKit: // 16 × 16 × 2.5: hitbox 40 × 40
        return frame(skin.repairKits, {0, 0}, {16, 16}, 2.5F);
    case Look::PlayerShip: // Drawn by widgets::ship
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
