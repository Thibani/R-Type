#include "client/Assets.hpp"

#include "client/Ui/Theme.hpp"

#include <SFML/Graphics/Image.hpp>

#include <cstddef>
#include <optional>
#include <system_error>
#include <utility>

namespace client {

namespace {

/// A 12 × 12 tile of player `index`'s swatch pattern: stripes, dots, checks
/// or solid colour (from the mockup's style guide).
sf::Image patternTile(std::size_t index)
{
    constexpr unsigned Size = 12;
    const sf::Color colour = theme::Players.at(index);
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

bool loadTexture(sf::Texture& texture, const std::filesystem::path& path)
{
    if (!texture.loadFromFile(path)) {
        return false;
    }
    texture.setSmooth(false); // Pixel art stays sharp when enlarged
    return true;
}

} // namespace

std::expected<void, std::string> Assets::load(Assets& assets, const std::filesystem::path& folder)
{
    const auto fail = [](const std::filesystem::path& path) {
        return std::unexpected("cannot load '" + path.string() + "'");
    };

    const std::filesystem::path sprites = folder / "sprites";
    for (const auto& [texture, file] : {std::pair{&assets.ships, "r-typesheet42.gif"},
             std::pair{&assets.bydos, "r-typesheet5.gif"},
             std::pair{&assets.missiles, "r-typesheet1.gif"},
             std::pair{&assets.pataPatas, "r-typesheet8.gif"},
             std::pair{&assets.bugs, "r-typesheet16.gif"},
             std::pair{&assets.cancers, "r-typesheet19.gif"},
             std::pair{&assets.cruisers, "r-typesheet24.gif"},
             std::pair{&assets.hoppers, "r-typesheet7.gif"},
             std::pair{&assets.spinners, "r-typesheet13.gif"},
             std::pair{&assets.gunners, "r-typesheet14.gif"},
             std::pair{&assets.mines, "r-typesheet39.gif"},
             std::pair{&assets.bosses, "r-typesheet30.gif"}}) {
        if (!loadTexture(*texture, sprites / file)) {
            return fail(sprites / file);
        }
    }
    for (std::size_t i = 0; i < assets.patterns.size(); ++i) {
        if (!assets.patterns.at(i).loadFromImage(patternTile(i))) {
            return std::unexpected("cannot create the player patterns");
        }
        assets.patterns.at(i).setRepeated(true);
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

} // namespace

std::optional<SpriteFrame> spriteFrame(const Assets& assets, Look look, std::uint8_t player)
{
    switch (look) {
    case Look::PlayerShip: {
        // Rows of r-typesheet42: light blue, pink, green, red (then blue, unused).
        // Column 2 is the ship flying level. 33 × 17 × 2 ≈ the server's 64 × 32 hitbox.
        const int row = player < theme::Players.size() ? player : 0;
        return frame(assets.ships, {66, row * 17}, {33, 17}, 2.0F);
    }
    case Look::Bydo:
        return frame(assets.bydos, {5, 6}, {21, 24}, 2.5F);
    case Look::PlayerMissile:
        return frame(assets.missiles, {249, 105}, {16, 8}, 2.0F);
    case Look::EnemyMissile:
        return frame(assets.missiles, {243, 277}, {12, 10}, 2.0F);
    // Enemies: the sprite is about the size of the server's hitbox (game/Enemies.hpp)
    case Look::PataPata: // 22 × 22 × 2 = 44: hitbox 40 × 40
        return frame(assets.pataPatas, {7, 7}, {22, 22}, 2.0F);
    case Look::Bug: // Head first, flying left. 16 × 9 × 3: hitbox 44 × 24
        return frame(assets.bugs, {324, 12}, {16, 9}, 3.0F);
    case Look::Cancer: // Facing the player. 24 × 21 × 2.5: hitbox 56 × 48
        return frame(assets.cancers, {103, 6}, {24, 21}, 2.5F);
    case Look::Cruiser: // Drawn facing right in the sheet. 48 × 48 × 2.5: hitbox 108 × 108
        return frame(assets.cruisers, {9, 9}, {48, 48}, 2.5F, true);
    case Look::Hopper: // Standing. 32 × 29 × 2.5: hitbox 80 × 80
        return frame(assets.hoppers, {1, 4}, {32, 29}, 2.5F);
    case Look::Spinner: // 32 × 29 × 2.5: hitbox 80 × 72
        return frame(assets.spinners, {1, 3}, {32, 29}, 2.5F);
    case Look::Gunner: // Gun forward. 48 × 47 × 2: hitbox 88 × 88
        return frame(assets.gunners, {1, 2}, {48, 47}, 2.0F);
    case Look::Mine: // 64 × 64 × 1.7: hitbox 104 × 104
        return frame(assets.mines, {34, 2}, {64, 64}, 1.7F);
    case Look::Boss: // 155 × 204 × 2: hitbox 240 × 360 (its body, not the tail tips)
        return frame(assets.bosses, {27, 2}, {155, 204}, 2.0F);
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
