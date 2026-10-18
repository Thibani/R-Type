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
             std::pair{&assets.missiles, "r-typesheet1.gif"}}) {
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

std::optional<SpriteFrame> spriteFrame(const Assets& assets, Look look, std::uint8_t player)
{
    switch (look) {
    case Look::PlayerShip: {
        // Rows of r-typesheet42: light blue, pink, green, red (then blue, unused).
        // Column 2 is the ship flying level. 33 × 17 × 2 ≈ the server's 64 × 32 hitbox.
        const int row = player < theme::Players.size() ? player : 0;
        return SpriteFrame{&assets.ships, sf::IntRect({66, row * 17}, {33, 17}), 2.0F};
    }
    case Look::Bydo:
        return SpriteFrame{&assets.bydos, sf::IntRect({5, 6}, {21, 24}), 2.5F};
    case Look::PlayerMissile:
        return SpriteFrame{&assets.missiles, sf::IntRect({249, 105}, {16, 8}), 2.0F};
    case Look::EnemyMissile:
        return SpriteFrame{&assets.missiles, sf::IntRect({243, 277}, {12, 10}), 2.0F};
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
