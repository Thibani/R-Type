#include "client/Ui/Widgets.hpp"

#include "client/Assets.hpp"
#include "client/Ui/Theme.hpp"

#include <SFML/Graphics/ConvexShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/System/String.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
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

template <std::size_t Count>
void polygon(sf::RenderTarget& target, const std::array<sf::Vector2f, Count>& points,
    sf::Color fill, sf::Color border, float borderWidth)
{
    sf::ConvexShape shape(Count);
    for (std::size_t i = 0; i < Count; ++i) {
        shape.setPoint(i, points.at(i));
    }
    shape.setFillColor(fill);
    shape.setOutlineColor(border);
    shape.setOutlineThickness(borderWidth);
    target.draw(shape);
}

/// The 5 corners of a banner, clockwise from the top left.
std::array<sf::Vector2f, 5> bannerPoints(sf::FloatRect rect, float tip)
{
    const auto [x, y] = rect.position;
    const auto [width, height] = rect.size;
    return {sf::Vector2f{x, y}, {x + width, y}, {x + width, y + height - tip},
        {x + (width / 2.0F), y + height}, {x, y + height - tip}};
}

sf::Text makeText(const sf::Font& font, std::string_view text, unsigned size, sf::Color colour)
{
    sf::Text drawable(font, sf::String::fromUtf8(text.begin(), text.end()), size);
    drawable.setFillColor(colour);
    return drawable;
}

} // namespace

void box(sf::RenderTarget& target, sf::FloatRect rect, sf::Color fill, sf::Color border,
    float borderWidth)
{
    sf::RectangleShape shape(rect.size);
    shape.setPosition(rect.position);
    shape.setFillColor(fill);
    shape.setOutlineColor(border);
    shape.setOutlineThickness(borderWidth);
    target.draw(shape);
}

void panel(sf::RenderTarget& target, sf::FloatRect rect, sf::Color fill, sf::Color border,
    float borderWidth, float cut)
{
    const auto [x, y] = rect.position;
    const auto [width, height] = rect.size;
    polygon<6>(target,
        {sf::Vector2f{x + cut, y}, {x + width, y}, {x + width, y + height - cut},
            {x + width - cut, y + height}, {x, y + height}, {x, y + cut}},
        fill, border, borderWidth);
}

void banner(sf::RenderTarget& target, sf::FloatRect rect, sf::Color fill, sf::Color border)
{
    constexpr float Tip = 62.0F;
    constexpr float Inset = 9.0F;
    polygon<5>(target, bannerPoints(rect, Tip), fill, border, 2.0F);
    const sf::FloatRect inner{
        rect.position + sf::Vector2f{Inset, Inset}, rect.size - sf::Vector2f{2 * Inset, 2 * Inset}};
    polygon<5>(target, bannerPoints(inner, Tip - 4.0F), sf::Color::Transparent, theme::Hull, 1.0F);
}

void goldDetail(sf::RenderTarget& target, sf::Vector2f position, float width)
{
    constexpr float Slant = 60.0F; // Degrees from the horizontal
    constexpr float Stroke = 11.0F;
    constexpr float Step = 9.0F;
    sf::RectangleShape line({Stroke, 1.5F});
    line.setFillColor(theme::Gold);
    line.setRotation(sf::degrees(-Slant));
    for (int i = 0; i < 3; ++i) {
        line.setPosition(position + sf::Vector2f{static_cast<float>(i) * Step, 10.0F});
        target.draw(line);
    }
    fillRect(target, {position + sf::Vector2f{32.0F, 0.0F}, {width - 32.0F, 1.5F}}, theme::Gold);
}

float textWidth(const sf::Font& font, std::string_view text, unsigned size)
{
    return makeText(font, text, size, sf::Color::White).getLocalBounds().size.x;
}

unsigned fittedSize(const sf::Font& font, std::string_view text, unsigned size, float width)
{
    constexpr unsigned smallest = 9;
    while (size > smallest && textWidth(font, text, size) > width) {
        --size;
    }
    return size;
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
    const sf::Color border = focused ? theme::Blue : theme::Hull;
    panel(target, rect, primary ? theme::Primary : theme::Surface, border, focused ? 3.0F : 1.5F,
        8.0F);
    centredText(target, assets.bodyBold, label, 20, primary ? theme::OnPrimary : theme::Text, rect);
}

namespace {

/// The end of `value` that fits in `width`: a field scrolls with what is typed.
std::string_view endThatFits(
    const sf::Font& font, std::string_view value, unsigned size, float width)
{
    const auto isContinuation
        = [](char c) { return (static_cast<unsigned char>(c) & 0xC0U) == 0x80U; };
    while (!value.empty() && textWidth(font, value, size) > width) {
        value.remove_prefix(1);
        while (!value.empty() && isContinuation(value.front())) { // Whole UTF-8 characters only
            value.remove_prefix(1);
        }
    }
    return value;
}

} // namespace

void field(sf::RenderTarget& target, const Assets& assets, sf::FloatRect rect,
    std::string_view label, std::string_view value, bool focused, std::string_view hint)
{
    constexpr float LabelHeight = 25.0F;
    const sf::Vector2f top = rect.position - sf::Vector2f{0.0F, LabelHeight};
    text(target, assets.body, label, 15, theme::Muted, top);
    if (!hint.empty()) {
        const float width = textWidth(assets.body, hint, 15);
        text(target, assets.body, hint, 15, theme::Muted,
            top + sf::Vector2f{rect.size.x - width, 0.0F});
    }
    box(target, rect, theme::InputBackground, focused ? theme::Blue : theme::Hull,
        focused ? 2.0F : 1.0F);
    const sf::Vector2f inside = rect.position + sf::Vector2f{14.0F, 12.0F};
    const std::string_view shown = endThatFits(assets.body, value, 19, rect.size.x - 34.0F);
    text(target, assets.body, shown, 19, theme::Text, inside);
    if (focused) {
        const float caretX = inside.x + textWidth(assets.body, shown, 19) + 3.0F;
        fillRect(
            target, {{caretX, rect.position.y + 10.0F}, {2.0F, rect.size.y - 20.0F}}, theme::Blue);
    }
}

void tag(sf::RenderTarget& target, const Assets& assets, std::string_view label,
    sf::Vector2f centre, unsigned size)
{
    const float width = textWidth(assets.bodyBold, label, size) + 12.0F;
    const float height = static_cast<float>(size) + 6.0F;
    const sf::FloatRect rect{centre - sf::Vector2f{width / 2.0F, height / 2.0F}, {width, height}};
    box(target, rect, theme::Surface, theme::Hull, 1.0F);
    centredText(target, assets.bodyBold, label, size, theme::Text, rect);
}

void pattern(sf::RenderTarget& target, const Assets& assets, ColourMode mode, sf::FloatRect rect,
    std::uint8_t player)
{
    const auto& patterns = assets.skins.at(static_cast<std::size_t>(mode)).patterns;
    if (player >= patterns.size()) {
        return;
    }
    sf::RectangleShape shape(rect.size);
    shape.setPosition(rect.position);
    shape.setTexture(&patterns.at(player));
    // A texture rect larger than the 12 × 12 tile repeats it
    shape.setTextureRect(sf::IntRect({0, 0}, sf::Vector2i(rect.size)));
    target.draw(shape);
}

void swatch(sf::RenderTarget& target, const Assets& assets, ColourMode mode, sf::FloatRect rect,
    std::uint8_t player, bool active)
{
    box(target, rect, theme::InputBackground, theme::Hull, 1.0F);
    if (active) {
        pattern(target, assets, mode, rect, player);
    }
    tag(target, assets, "P" + std::to_string(player + 1), rect.getCenter(), 18);
}

void ship(sf::RenderTarget& target, const Assets& assets, ColourMode mode, sf::FloatRect rect,
    std::uint8_t player)
{
    const auto frame = spriteFrame(assets, mode, Look::PlayerShip, player);
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
