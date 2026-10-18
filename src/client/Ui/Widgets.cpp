#include "client/Ui/Widgets.hpp"

#include "client/Assets.hpp"
#include "client/Ui/Theme.hpp"

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/System/String.hpp>

#include <algorithm>
#include <string>

namespace client::widgets {

namespace {

void fillRect(sf::RenderTarget& target, sf::FloatRect rect, sf::Color colour)
{
    sf::RectangleShape shape(rect.size);
    shape.setPosition(rect.position);
    shape.setFillColor(colour);
    target.draw(shape);
}

sf::Text makeText(const sf::Font& font, std::string_view text, unsigned size, sf::Color colour)
{
    sf::Text drawable(font, sf::String::fromUtf8(text.begin(), text.end()), size);
    drawable.setFillColor(colour);
    return drawable;
}

} // namespace

void box(sf::RenderTarget& target, sf::FloatRect rect, sf::Color fill, sf::Color border,
    float borderWidth, bool shadow)
{
    const float w = borderWidth;
    const auto [x, y] = rect.position;
    const auto [width, height] = rect.size;
    if (shadow) {
        const float offset = w + 6.0F;
        fillRect(target, {{x + offset, y + offset}, {width, height}}, theme::HullLight);
    }
    fillRect(target, rect, fill);
    // Border drawn outside the rect, with empty corners: the pixel-art look
    fillRect(target, {{x, y - w}, {width, w}}, border);
    fillRect(target, {{x, y + height}, {width, w}}, border);
    fillRect(target, {{x - w, y}, {w, height}}, border);
    fillRect(target, {{x + width, y}, {w, height}}, border);
}

float textWidth(const sf::Font& font, std::string_view text, unsigned size)
{
    return makeText(font, text, size, sf::Color::White).getLocalBounds().size.x;
}

void text(sf::RenderTarget& target, const sf::Font& font, std::string_view text, unsigned size,
    sf::Color colour, sf::Vector2f position)
{
    sf::Text drawable = makeText(font, text, size, colour);
    // Glyphs start below the text's origin: shift so the visible top is at `position`
    drawable.setPosition(position - drawable.getLocalBounds().position);
    target.draw(drawable);
}

void centredText(sf::RenderTarget& target, const sf::Font& font, std::string_view text,
    unsigned size, sf::Color colour, sf::FloatRect area)
{
    sf::Text drawable = makeText(font, text, size, colour);
    const sf::FloatRect bounds = drawable.getLocalBounds();
    drawable.setPosition(area.position + (area.size - bounds.size) / 2.0F - bounds.position);
    target.draw(drawable);
}

void button(sf::RenderTarget& target, const Assets& assets, sf::FloatRect rect,
    std::string_view label, bool primary, bool focused)
{
    if (focused) {
        // Focus ring: 4 px, 8 px away, light blue (visible on light and dark screens)
        constexpr float Gap = 8.0F;
        box(target,
            {rect.position - sf::Vector2f{Gap, Gap}, rect.size + sf::Vector2f{2 * Gap, 2 * Gap}},
            sf::Color::Transparent, theme::LightBlue, 4.0F);
    }
    if (primary) {
        box(target, rect, theme::Blue, theme::DarkBlue, 4.0F);
        centredText(target, assets.display, label, 22, sf::Color::White, rect);
    } else {
        box(target, rect, theme::Surface, theme::Hull, 4.0F);
        centredText(target, assets.bodyBold, label, 16, theme::Text, rect);
    }
}

void field(sf::RenderTarget& target, const Assets& assets, sf::FloatRect rect,
    std::string_view label, std::string_view value, bool focused)
{
    text(
        target, assets.bodyBold, label, 16, theme::Text, rect.position - sf::Vector2f{0.0F, 28.0F});
    box(target, rect, theme::InputBackground, focused ? theme::Blue : theme::Hull, 3.0F);
    const sf::Vector2f inside = rect.position + sf::Vector2f{14.0F, 14.0F};
    text(target, assets.body, value, 19, theme::Text, inside);
    if (focused) {
        const float caretX = inside.x + textWidth(assets.body, value, 19) + 3.0F;
        fillRect(
            target, {{caretX, rect.position.y + 10.0F}, {3.0F, rect.size.y - 20.0F}}, theme::Blue);
    }
}

void tag(sf::RenderTarget& target, const Assets& assets, std::string_view label,
    sf::Vector2f centre, unsigned size)
{
    const float width = textWidth(assets.display, label, size) + 12.0F;
    const float height = static_cast<float>(size) + 6.0F;
    const sf::FloatRect rect{centre - sf::Vector2f{width / 2.0F, height / 2.0F}, {width, height}};
    box(target, rect, theme::Surface, theme::Hull, 2.0F);
    centredText(target, assets.display, label, size, theme::Text, rect);
}

void swatch(sf::RenderTarget& target, const Assets& assets, sf::FloatRect rect, std::uint8_t player,
    bool active)
{
    box(target, rect, theme::InputBackground, theme::Hull, 3.0F);
    if (active && player < assets.patterns.size()) {
        sf::RectangleShape pattern(rect.size);
        pattern.setPosition(rect.position);
        pattern.setTexture(&assets.patterns.at(player));
        // A texture rect larger than the 12 × 12 tile repeats it
        pattern.setTextureRect(sf::IntRect({0, 0}, sf::Vector2i(rect.size)));
        target.draw(pattern);
    }
    tag(target, assets, "P" + std::to_string(player + 1), rect.getCenter(), 18);
}

void ship(sf::RenderTarget& target, const Assets& assets, sf::FloatRect rect, std::uint8_t player)
{
    const auto frame = spriteFrame(assets, Look::PlayerShip, player);
    if (!frame) {
        return;
    }
    sf::Sprite sprite(*frame->texture, frame->rect);
    const sf::Vector2f size(frame->rect.size);
    const float scale = std::min(rect.size.x / size.x, rect.size.y / size.y);
    sprite.setScale({scale, scale});
    sprite.setPosition(rect.getCenter() - size * scale / 2.0F);
    target.draw(sprite);
}

} // namespace client::widgets
