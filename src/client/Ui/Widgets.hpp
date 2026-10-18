#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

#include <cstdint>
#include <string_view>

namespace client {

struct Assets;

/// Small drawing helpers for the Hangar style, used by the interface system.
/// Texts are UTF-8; positions are on the 1280 × 720 interface canvas.
namespace widgets {

/// A pixel-art panel: square border without corners, optional hard shadow.
void box(sf::RenderTarget& target, sf::FloatRect rect, sf::Color fill, sf::Color border,
    float borderWidth, bool shadow = false);

/// Width of `text` once drawn.
[[nodiscard]]
float textWidth(const sf::Font& font, std::string_view text, unsigned size);

/// Draws `text` with its top-left corner at `position`.
void text(sf::RenderTarget& target, const sf::Font& font, std::string_view text, unsigned size,
    sf::Color colour, sf::Vector2f position);

/// Draws `text` centred in `area`.
void centredText(sf::RenderTarget& target, const sf::Font& font, std::string_view text,
    unsigned size, sf::Color colour, sf::FloatRect area);

/// Primary (blue) or secondary (white) button, with the focus ring if focused.
void button(sf::RenderTarget& target, const Assets& assets, sf::FloatRect rect,
    std::string_view label, bool primary, bool focused);

/// Text field: label above, value inside, caret when focused.
void field(sf::RenderTarget& target, const Assets& assets, sf::FloatRect rect,
    std::string_view label, std::string_view value, bool focused);

/// "P1"-style tag: white box, display font.
void tag(sf::RenderTarget& target, const Assets& assets, std::string_view label,
    sf::Vector2f centre, unsigned size);

/// Player swatch: colour + pattern + tag, so players differ without colour.
void swatch(sf::RenderTarget& target, const Assets& assets, sf::FloatRect rect, std::uint8_t player,
    bool active);

/// The ship of `player`, enlarged to fit `rect`.
void ship(sf::RenderTarget& target, const Assets& assets, sf::FloatRect rect, std::uint8_t player);

} // namespace widgets
} // namespace client
