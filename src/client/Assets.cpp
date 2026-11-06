#include "client/Assets.hpp"

#include "client/Ui/Theme.hpp"

#include <SFML/Graphics/Image.hpp>

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
bool loadSkin(Skin& skin, ColourMode mode, sf::Image ships, sf::Image bydos, sf::Image missiles)
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
    for (std::size_t mode = 0; mode < assets.skins.size(); ++mode) {
        if (!loadSkin(assets.skins.at(mode), static_cast<ColourMode>(mode), ships, bydos, missiles)) {
            return std::unexpected("cannot create the textures of the colour modes");
        }
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

std::optional<SpriteFrame> spriteFrame(
    const Assets& assets, ColourMode mode, Look look, std::uint8_t player)
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
