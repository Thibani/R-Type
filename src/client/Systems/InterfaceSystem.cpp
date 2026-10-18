#include "client/Systems/InterfaceSystem.hpp"

#include "client/Assets.hpp"
#include "client/Components/NetEntity.hpp"
#include "client/Components/Session.hpp"
#include "client/Singleton.hpp"
#include "client/Ui/Layout.hpp"
#include "client/Ui/Theme.hpp"
#include "client/Ui/View.hpp"
#include "client/Ui/Widgets.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"

#include <SFML/Graphics/RectangleShape.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace client {

namespace {

using widgets::box;
using widgets::centredText;
using widgets::text;
using widgets::textWidth;

/// "12450" → "12 450", as in the mockups.
std::string formatScore(std::uint32_t score)
{
    std::string digits = std::to_string(score);
    for (std::ptrdiff_t i = std::ssize(digits) - 3; i > 0; i -= 3) {
        digits.insert(static_cast<std::size_t>(i), " ");
    }
    return digits;
}

std::string playerLabel(std::size_t player)
{
    return "P" + std::to_string(player + 1);
}

/// Light background with the 64 px grid of the mockups.
void drawLightBackground(sf::RenderTarget& target)
{
    box(target, {{0.0F, 0.0F}, {theme::UiWidth, theme::UiHeight}}, theme::Background,
        theme::Background, 0.0F);
    sf::RectangleShape line;
    line.setFillColor(theme::Grid);
    line.setSize({2.0F, theme::UiHeight});
    for (float x = 0.0F; x < theme::UiWidth; x += 64.0F) {
        line.setPosition({x, 0.0F});
        target.draw(line);
    }
    line.setSize({theme::UiWidth, 2.0F});
    for (float y = 0.0F; y < theme::UiHeight; y += 64.0F) {
        line.setPosition({0.0F, y});
        target.draw(line);
    }
}

/// Big title in the display font with its hard light-blue shadow.
void title(sf::RenderTarget& target, const Assets& assets, std::string_view label, unsigned size,
    sf::Vector2f position)
{
    const float shadow = size >= 100 ? 6.0F : 4.0F;
    text(target, assets.display, label, size, theme::LightBlue,
        position + sf::Vector2f{shadow, shadow});
    text(target, assets.display, label, size, theme::Blue, position);
}

void errorBox(
    sf::RenderTarget& target, const Assets& assets, sf::FloatRect rect, std::string_view message)
{
    box(target, rect, theme::ErrorBackground, theme::Red, 3.0F);
    text(target, assets.bodyBold, message, 16, theme::Red,
        rect.position + sf::Vector2f{14.0F, 13.0F});
}

/// The coloured top of a player card: pattern, tag, and "YOU" for us.
void cardHead(sf::RenderTarget& target, const Assets& assets, sf::FloatRect rect,
    std::size_t player, bool filled, bool isYou)
{
    if (filled) {
        widgets::swatch(target, assets, rect, static_cast<std::uint8_t>(player), true);
    } else {
        box(target, rect, theme::InputBackground, theme::InputBackground, 0.0F);
        widgets::tag(target, assets, playerLabel(player), rect.getCenter(), 18);
    }
    box(target, {{rect.position.x, rect.position.y + rect.size.y}, {rect.size.x, 0.0F}},
        theme::Hull, theme::Hull, 2.0F);
    if (isYou) {
        const sf::FloatRect you{
            {rect.position.x + rect.size.x - 60.0F, rect.position.y + 10.0F}, {48.0F, 24.0F}};
        box(target, you, theme::Blue, theme::Blue, 0.0F);
        centredText(target, assets.display, "YOU", 15, sf::Color::White, you);
    }
}

void drawConnect(sf::RenderTarget& target, const Assets& assets, const Session& session)
{
    drawLightBackground(target);
    title(target, assets, "R-TYPE", 150, {80.0F, 220.0F});
    widgets::ship(target, assets, {{80.0F, 417.0F}, {192.0F, 72.0F}}, 0);

    box(target, layout::ConnectPanel, theme::Surface, theme::Hull, 4.0F, true);
    text(target, assets.display, "Join a game", 32, theme::Text, {700.0F, 164.0F});
    widgets::field(target, assets, layout::NameInput, "Player name", session.name,
        session.focus == layout::NameField);
    text(target, assets.body, "16 characters max", 15, theme::Muted, {700.0F, 303.0F});
    widgets::field(target, assets, layout::AddressInput, "Server address", session.address,
        session.focus == layout::AddressField);
    widgets::field(target, assets, layout::PortInput, "Port", session.port,
        session.focus == layout::PortField);
    widgets::button(
        target, assets, layout::Connect, "Connect", true, session.focus == layout::ConnectButton);
    widgets::button(
        target, assets, layout::Quit, "Quit", false, session.focus == layout::QuitButton);

    if (!session.error.empty()) {
        errorBox(target, assets, {{700.0F, 515.0F}, {468.0F, 44.0F}}, session.error);
    } else if (!session.status.empty()) {
        text(target, assets.bodyBold, session.status, 17, theme::Blue, {700.0F, 527.0F});
    }
    text(target, assets.body, "Tab: next field  ·  Enter: connect  ·  Esc: quit", 15, theme::Muted,
        {80.0F, 682.0F});
}

void drawWaitingRoom(sf::RenderTarget& target, const Assets& assets, const Session& session)
{
    drawLightBackground(target);
    title(target, assets, "Waiting room", 60, {80.0F, 56.0F});
    const auto connected = std::ranges::count_if(
        session.players, [](const PlayerSlot& slot) { return slot.present; });
    text(target, assets.bodyBold,
        std::to_string(connected) + " / " + std::to_string(session.players.size())
            + " players connected. Waiting for the server to start the game...",
        17, theme::Blue, {80.0F, 134.0F});

    constexpr float Width = 259.0F;
    constexpr float Gap = 28.0F;
    for (std::size_t i = 0; i < session.players.size(); ++i) {
        const PlayerSlot& slot = session.players.at(i);
        const sf::FloatRect card{
            {80.0F + static_cast<float>(i) * (Width + Gap), 178.0F}, {Width, 400.0F}};
        box(target, card, theme::Surface, theme::Hull, slot.present ? 4.0F : 3.0F, slot.present);
        cardHead(target, assets, {card.position, {Width, 88.0F}}, i, slot.present,
            session.playerId == i);
        if (slot.present) {
            widgets::ship(target, assets, {{card.position.x + 33.0F, 298.0F}, {192.0F, 72.0F}},
                static_cast<std::uint8_t>(i));
            centredText(target, assets.display, slot.name, 24, theme::Text,
                {{card.position.x, 390.0F}, {Width, 30.0F}});
        } else {
            centredText(target, assets.body, "Free slot", 15, theme::Muted,
                {{card.position.x, 266.0F}, {Width, 312.0F}});
        }
    }
    widgets::button(target, assets, layout::Leave, "Leave", false, session.focus == 0);
    const std::string server = "Server " + session.address + ":" + session.port + "  ·  Esc: leave";
    text(target, assets.body, server, 15, theme::Muted,
        {1200.0F - textWidth(assets.body, server, 15), 628.0F});
}

/// Health of each player's ship, from the snapshots (0 if no ship).
std::array<std::uint8_t, 4> shipHealth(ecs::World& world)
{
    std::array<std::uint8_t, 4> health{};
    world.query<NetEntity>([&](ecs::Entity /*entity*/, NetEntity& data) {
        if (data.kind == network::protocol::EntityKind::PlayerShip && data.owner < health.size()) {
            health.at(data.owner) = data.health;
        }
    });
    return health;
}

void drawHud(sf::RenderTarget& target, const Assets& assets, ecs::World& world,
    const Session& session, float now)
{
    // Team score, top left
    const std::string score = formatScore(session.score);
    const float scoreWidth = std::max(textWidth(assets.bodyBold, "TEAM SCORE", 13),
                                 textWidth(assets.display, score, 32))
        + 36.0F;
    box(target, {{20.0F, 20.0F}, {scoreWidth, 64.0F}}, theme::Surface, theme::Hull, 4.0F);
    text(target, assets.bodyBold, "TEAM SCORE", 13, theme::Muted, {38.0F, 29.0F});
    text(target, assets.display, score, 32, theme::Blue, {38.0F, 48.0F});

    // One chip per player, right-aligned: swatch, name, health or state
    const auto health = shipHealth(world);
    float right = theme::UiWidth - 20.0F;
    for (std::size_t back = session.players.size(); back > 0; --back) {
        const std::size_t i = back - 1;
        const PlayerSlot& slot = session.players.at(i);
        if (!slot.present && !slot.left) {
            continue;
        }
        const std::string name = session.playerId == i ? slot.name + " (you)" : slot.name;
        const float width
            = 8.0F + 56.0F + 12.0F + std::max(104.0F, textWidth(assets.bodyBold, name, 15)) + 14.0F;
        const sf::FloatRect chip{{right - width, 20.0F}, {width, 64.0F}};
        right -= width + 20.0F;
        box(target, chip, theme::Surface, theme::Hull, slot.left ? 3.0F : 4.0F);
        widgets::swatch(target, assets, {chip.position + sf::Vector2f{8.0F, 8.0F}, {56.0F, 48.0F}},
            static_cast<std::uint8_t>(i), slot.present && !slot.down);
        const sf::Vector2f column = chip.position + sf::Vector2f{76.0F, 10.0F};
        text(target, assets.bodyBold, name, 15, theme::Text, column);
        if (slot.left) {
            text(target, assets.body, "Disconnected", 14, theme::Muted,
                column + sf::Vector2f{0.0F, 26.0F});
        } else if (slot.down) {
            text(target, assets.bodyBold, "Down", 14, theme::Red,
                column + sf::Vector2f{0.0F, 26.0F});
        } else {
            const std::uint8_t points = health.at(i);
            for (std::uint8_t pip = 0; pip < 3; ++pip) {
                const sf::Color fill = pip >= points ? sf::Color::White
                    : points == 1                    ? theme::Red
                                                     : theme::Green;
                box(target,
                    {column + sf::Vector2f{2.0F + static_cast<float>(pip) * 28.0F, 30.0F},
                        {22.0F, 12.0F}},
                    fill, theme::Hull, 2.0F);
            }
        }
    }

    if (!session.notice.empty() && now < session.noticeUntil) {
        const float width = textWidth(assets.bodyBold, session.notice, 16) + 28.0F;
        const sf::FloatRect toast{{theme::UiWidth - 20.0F - width, 122.0F}, {width, 40.0F}};
        box(target, toast, theme::Blue, theme::LightBlue, 3.0F);
        text(target, assets.bodyBold, session.notice, 16, sf::Color::White,
            toast.position + sf::Vector2f{14.0F, 12.0F});
    }

    const std::string reminder = "Arrows: move  ·  Space: fire  ·  Esc: menu";
    const float reminderWidth = textWidth(assets.body, reminder, 15) + 24.0F;
    box(target, {{20.0F, 668.0F}, {reminderWidth, 32.0F}}, theme::Surface, theme::Hull, 3.0F);
    text(target, assets.body, reminder, 15, theme::Text, {32.0F, 677.0F});
}

void drawMenu(sf::RenderTarget& target, const Assets& assets, const Session& session)
{
    box(target, {{0.0F, 0.0F}, {theme::UiWidth, theme::UiHeight}}, theme::Overlay, theme::Overlay,
        0.0F);
    box(target, layout::MenuPanel, theme::Surface, theme::Hull, 4.0F, true);
    text(target, assets.display, "Menu", 32, theme::Text, {462.0F, 246.0F});
    text(target, assets.body, "The game keeps running for the other players.", 15, theme::Muted,
        {462.0F, 300.0F});
    widgets::button(target, assets, layout::Resume, "Resume", true, session.focus == 0);
    widgets::button(target, assets, layout::LeaveGame, "Leave game", false, session.focus == 1);
}

void drawGameOver(sf::RenderTarget& target, const Assets& assets, const Session& session)
{
    drawLightBackground(target);
    const float titleWidth = textWidth(assets.display, "GAME OVER", 150);
    title(target, assets, "GAME OVER", 150, {(theme::UiWidth - titleWidth) / 2.0F, 60.0F});

    const std::string score = formatScore(session.score);
    const float boxWidth = std::max(textWidth(assets.display, score, 72), 160.0F) + 72.0F;
    const sf::FloatRect scoreBox{{(theme::UiWidth - boxWidth) / 2.0F, 248.0F}, {boxWidth, 130.0F}};
    box(target, scoreBox, theme::Surface, theme::Hull, 4.0F, true);
    centredText(target, assets.bodyBold, "TEAM SCORE", 13, theme::Muted,
        {scoreBox.position + sf::Vector2f{0.0F, 18.0F}, {boxWidth, 16.0F}});
    centredText(target, assets.display, score, 72, theme::Blue,
        {scoreBox.position + sf::Vector2f{0.0F, 42.0F}, {boxWidth, 72.0F}});

    // The crew: the players still in the game
    std::vector<std::size_t> crew;
    for (std::size_t i = 0; i < session.players.size(); ++i) {
        if (session.players.at(i).present) {
            crew.push_back(i);
        }
    }
    constexpr float Gap = 24.0F;
    constexpr float GridWidth = 740.0F;
    const float count = static_cast<float>(std::max<std::size_t>(crew.size(), 1));
    const float width = (GridWidth - Gap * (count - 1.0F)) / count;
    for (std::size_t n = 0; n < crew.size(); ++n) {
        const std::size_t i = crew.at(n);
        const sf::FloatRect card{
            {270.0F + static_cast<float>(n) * (width + Gap), 412.0F}, {width, 160.0F}};
        box(target, card, theme::Surface, theme::Hull, 4.0F, true);
        cardHead(target, assets, {card.position, {width, 52.0F}}, i, true, false);
        widgets::ship(target, assets,
            {{card.getCenter().x - 48.0F, card.position.y + 68.0F}, {96.0F, 36.0F}},
            static_cast<std::uint8_t>(i));
        centredText(target, assets.bodyBold, session.players.at(i).name, 15, theme::Text,
            {{card.position.x, card.position.y + 118.0F}, {width, 24.0F}});
    }
    widgets::button(target, assets, layout::BackToMenu, "Back to menu", true, session.focus == 0);
}

} // namespace

InterfaceSystem::InterfaceSystem(sf::RenderWindow& window, const Assets& assets) noexcept
    : _window(window)
    , _assets(assets)
{
}

void InterfaceSystem::update(ecs::World& world, const ecs::Time& time)
{
    const Session& session = singleton<Session>(world);
    _window.setView(letterboxed({theme::UiWidth, theme::UiHeight}, _window.getSize()));
    switch (session.screen) {
    case Screen::Connect:
        drawConnect(_window, _assets, session);
        break;
    case Screen::WaitingRoom:
        drawWaitingRoom(_window, _assets, session);
        break;
    case Screen::Game:
        drawHud(_window, _assets, world, session, time.elapsedSeconds());
        if (session.menuOpen) {
            drawMenu(_window, _assets, session);
        }
        break;
    case Screen::GameOver:
        drawGameOver(_window, _assets, session);
        break;
    }
    _window.display();
}

} // namespace client
