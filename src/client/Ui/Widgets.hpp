#pragma once

#include "client/Components/Settings.hpp"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

#include <cstdint>
#include <string_view>

namespace client {

struct Assets;

/// Small drawing helpers for the dark style, used by the interface system.
/// Texts are UTF-8; positions are on the 1280 × 720 interface canvas.
namespace widgets {

/// A plain rectangle with a border around it.
void box(sf::RenderTarget& target, sf::FloatRect rect, sf::Color fill, sf::Color border,
    float borderWidth);

/// A panel with its top-left and bottom-right corners cut (`cut` pixels).
void panel(sf::RenderTarget& target, sf::FloatRect rect, sf::Color fill, sf::Color border,
    float borderWidth, float cut = 14.0F);

/// A vertical banner ending in a point at the bottom, with a thin inner line.
void banner(sf::RenderTarget& target, sf::FloatRect rect, sf::Color fill, sf::Color border);

/// Decoration: three short slanted gold lines and a thin line after them.
void goldDetail(sf::RenderTarget& target, sf::Vector2f position, float width);

/// Width of `text` once drawn.
[[nodiscard]]
float textWidth(const sf::Font& font, std::string_view text, unsigned size);

/// The largest size up to `size` at which `text` fits in `width`.
[[nodiscard]]
unsigned fittedSize(const sf::Font& font, std::string_view text, unsigned size, float width);

/// Draws `text` with its top-left corner at `position`.
void text(sf::RenderTarget& target, const sf::Font& font, std::string_view text, unsigned size,
    sf::Color colour, sf::Vector2f position);

/// Draws `text` centred in `area`.
void centredText(sf::RenderTarget& target, const sf::Font& font, std::string_view text,
    unsigned size, sf::Color colour, sf::FloatRect area);

/// Primary (orange) or secondary (dark) button, outlined in cyan if focused.
void button(sf::RenderTarget& target, const Assets& assets, sf::FloatRect rect,
    std::string_view label, bool primary, bool focused);

/// Text field: label above (and `hint` at its right), value inside, caret when focused.
void field(sf::RenderTarget& target, const Assets& assets, sf::FloatRect rect,
    std::string_view label, std::string_view value, bool focused, std::string_view hint = {});

/// "P1"-style tag: small box, bold text.
void tag(sf::RenderTarget& target, const Assets& assets, std::string_view label,
    sf::Vector2f centre, unsigned size);

/// The repeating pattern of `player`, filling `rect`.
void pattern(sf::RenderTarget& target, const Assets& assets, ColourMode mode, sf::FloatRect rect,
    std::uint8_t player);

/// The ship of `player`, in their colour, as large as `rect` allows.
void ship(sf::RenderTarget& target, const Assets& assets, ColourMode mode, sf::FloatRect rect,
    std::uint8_t player);

} // namespace widgets
} // namespace client
