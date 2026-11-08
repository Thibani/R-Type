#include "client/Systems/InterfaceSystem.hpp"

#include "client/Assets.hpp"
#include "client/Components/NetEntity.hpp"
#include "client/Components/Session.hpp"
#include "client/Components/Settings.hpp"
#include "client/Gamepad.hpp"
#include "client/Settings.hpp"
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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>

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
void drawLightBackground(sf::RenderTarget& target, sf::FloatRect area)
{
    box(target, area, theme::Background, theme::Background, 0.0F);
    constexpr float cell = 64.0F;
    const sf::Vector2f end = area.position + area.size;
    sf::RectangleShape line;
    line.setFillColor(theme::Grid);
    line.setSize({2.0F, area.size.y});
    const float firstX = std::floor(area.position.x / cell) * cell;
    for (int column = 0; firstX + (static_cast<float>(column) * cell) < end.x; ++column) {
        line.setPosition({firstX + (static_cast<float>(column) * cell), area.position.y});
        target.draw(line);
    }
    line.setSize({area.size.x, 2.0F});
    const float firstY = std::floor(area.position.y / cell) * cell;
    for (int row = 0; firstY + (static_cast<float>(row) * cell) < end.y; ++row) {
        line.setPosition({area.position.x, firstY + (static_cast<float>(row) * cell)});
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
void cardHead(sf::RenderTarget& target, const Assets& assets, ColourMode mode, sf::FloatRect rect,
    std::size_t player, bool filled, bool isYou)
{
    if (filled) {
        widgets::swatch(target, assets, mode, rect, static_cast<std::uint8_t>(player), true);
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

/// One star per character of the password (a UTF-8 character can use several bytes).
std::string hidden(std::string_view password)
{
    const auto isCharacterStart
        = [](char c) { return (static_cast<unsigned char>(c) & 0xC0U) != 0x80U; };
    return std::string(
        static_cast<std::size_t>(std::ranges::count_if(password, isCharacterStart)), '*');
}

/// A small player card: head, ship and name, or "Free slot".
void crewCard(sf::RenderTarget& target, const Assets& assets, ColourMode mode, sf::FloatRect card,
    std::size_t player, const PlayerSlot& slot, bool isYou)
{
    box(target, card, theme::Surface, theme::Hull, slot.present ? 4.0F : 3.0F, slot.present);
    cardHead(target, assets, mode, {card.position, {card.size.x, 52.0F}}, player, slot.present, isYou);
    if (!slot.present) {
        centredText(target, assets.body, "Free slot", 15, theme::Muted,
            {card.position + sf::Vector2f{0.0F, 52.0F}, {card.size.x, card.size.y - 52.0F}});
        return;
    }
    widgets::ship(target, assets, mode,
        {{card.getCenter().x - 48.0F, card.position.y + 68.0F}, {96.0F, 36.0F}},
        static_cast<std::uint8_t>(player));
    centredText(target, assets.bodyBold, slot.name,
        widgets::fittedSize(assets.bodyBold, slot.name, 15, card.size.x - 12.0F), theme::Text,
        {{card.position.x, card.position.y + 118.0F}, {card.size.x, 24.0F}});
}

void drawConnect(sf::RenderTarget& target, const Assets& assets, ColourMode mode,
    const Session& session, sf::FloatRect area, bool compact)
{
    drawLightBackground(target, area);
    if (!compact) { // Then the view only shows the panel, with the hint inside it
        title(target, assets, "R-TYPE", 150, {80.0F, 220.0F});
        widgets::ship(target, assets, mode, {{80.0F, 417.0F}, {192.0F, 72.0F}}, 0);
    }

    box(target, layout::ConnectPanel, theme::Surface, theme::Hull, 4.0F, true);
    text(target, assets.display, "Join a game", 32, theme::Text, {700.0F, 146.0F});
    widgets::field(target, assets, layout::NameInput, "Player name", session.name,
        session.focus == layout::NameField);
    text(target, assets.body, "16 characters max", 15, theme::Muted, {700.0F, 271.0F});
    widgets::field(target, assets, layout::PasswordInput, "Password", hidden(session.password),
        session.focus == layout::PasswordField);
    text(target, assets.body, "8 to 64 characters", 15, theme::Muted, {942.0F, 271.0F});
    widgets::field(target, assets, layout::AddressInput, "Server address", session.address,
        session.focus == layout::AddressField);
    widgets::field(target, assets, layout::PortInput, "Port", session.port,
        session.focus == layout::PortField);
    widgets::button(
        target, assets, layout::Login, "Log in", true, session.focus == layout::LoginButton);
    widgets::button(target, assets, layout::CreateAccount, "Create account", false,
        session.focus == layout::CreateAccountButton);
    widgets::button(target, assets, layout::OpenSettings, "Settings", false,
        session.focus == layout::SettingsButton);
    widgets::button(
        target, assets, layout::Quit, "Quit", false, session.focus == layout::QuitButton);

    if (!session.error.empty()) {
        errorBox(target, assets, {{700.0F, 515.0F}, {468.0F, 44.0F}}, session.error);
    } else if (!session.status.empty()) {
        text(target, assets.bodyBold, session.status, 17, theme::Blue, {700.0F, 527.0F});
    }
    text(target, assets.body, "Tab: next field  ·  Enter: log in  ·  Esc: quit", 15, theme::Muted,
        compact ? sf::Vector2f{700.0F, 566.0F} : sf::Vector2f{80.0F, 682.0F});
}

void drawWaitingRoom(sf::RenderTarget& target, const Assets& assets, ColourMode mode,
    const Session& session, sf::FloatRect area, int interfaceSize)
{
    const bool compact = layout::isCompact(interfaceSize);
    drawLightBackground(target, area);
    const sf::Vector2f titleAt = compact ? sf::Vector2f{270.0F, 128.0F} : sf::Vector2f{80.0F, 56.0F};
    title(target, assets, "Waiting room", compact ? 40U : 60U, titleAt);
    const auto connected = std::ranges::count_if(
        session.players, [](const PlayerSlot& slot) { return slot.present; });
    text(target, assets.bodyBold,
        std::to_string(connected) + " / " + std::to_string(session.players.size())
            + " players connected. Waiting for the server to start the game...",
        17, theme::Blue, {titleAt.x, compact ? 178.0F : 134.0F});

    if (compact) {
        constexpr float width = 167.0F;
        constexpr float gap = 24.0F;
        for (std::size_t i = 0; i < session.players.size(); ++i) {
            crewCard(target, assets, mode,
                {{270.0F + (static_cast<float>(i) * (width + gap)), 215.0F}, {width, 160.0F}}, i,
                session.players.at(i), session.playerId == i);
        }
    } else {
        constexpr float width = 259.0F;
        constexpr float gap = 28.0F;
        for (std::size_t i = 0; i < session.players.size(); ++i) {
            const PlayerSlot& slot = session.players.at(i);
            const sf::FloatRect card{
                {80.0F + (static_cast<float>(i) * (width + gap)), 178.0F}, {width, 400.0F}};
            box(target, card, theme::Surface, theme::Hull, slot.present ? 4.0F : 3.0F, slot.present);
            cardHead(target, assets, mode, {card.position, {width, 88.0F}}, i, slot.present,
                session.playerId == i);
            if (slot.present) {
                widgets::ship(target, assets, mode,
                    {{card.position.x + 33.0F, 298.0F}, {192.0F, 72.0F}},
                    static_cast<std::uint8_t>(i));
                centredText(target, assets.bodyBold, slot.name,
                    widgets::fittedSize(assets.bodyBold, slot.name, 24, width - 16.0F), theme::Text,
                    {{card.position.x, 390.0F}, {width, 30.0F}});
            } else {
                centredText(target, assets.body, "Free slot", 15, theme::Muted,
                    {{card.position.x, 266.0F}, {width, 312.0F}});
            }
        }
    }
    widgets::button(
        target, assets, layout::leave(interfaceSize), "Leave", false, session.focus == 0);
    const std::string server = "Server " + session.address + ":" + session.port + "  ·  Esc: leave";
    text(target, assets.body, server, 15, theme::Muted,
        {(compact ? 1010.0F : 1200.0F) - textWidth(assets.body, server, 15), compact ? 420.0F : 628.0F});
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

/// "Arrows: move  ·  Space: fire  ·  Esc: menu", with the player's own keys.
std::string controlsReminder(const Settings& settings)
{
    const auto name = [&](Action action) {
        return std::string(keyName(settings.keys.at(static_cast<std::size_t>(action))).value_or("?"));
    };
    constexpr std::size_t moves = 4; // Up, Down, Left, Right come first in Action
    const bool arrows = std::ranges::equal(
        std::span(settings.keys).first<moves>(), std::span(Settings::DefaultKeys).first<moves>());
    const std::string move = arrows
        ? "Arrows: move"
        : "Move: " + name(Action::Up) + " " + name(Action::Left) + " " + name(Action::Down) + " "
            + name(Action::Right);
    std::string keyboard = move + "  ·  " + name(Action::Fire) + ": fire  ·  " + name(Action::Pod)
        + ": move the Force  ·  Esc: menu";
    const auto fire = settings.buttons.at(static_cast<std::size_t>(Action::Fire));
    const auto pod = settings.buttons.at(static_cast<std::size_t>(Action::Pod));
    if (!gamepad::anyConnected()) {
        return keyboard;
    }
    return keyboard + "  ·  Pad: stick"
        + (fire ? ", " + std::to_string(*fire) + ": fire" : std::string())
        + (pod ? ", " + std::to_string(*pod) + ": Force" : std::string()) + ", Start: menu";
}

/// One player in the HUD: swatch, name, then health or state.
void drawPlayerChip(sf::RenderTarget& target, const Assets& assets, ColourMode mode,
    sf::FloatRect chip, std::size_t player, const PlayerSlot& slot, const std::string& name,
    std::uint8_t points)
{
    box(target, chip, theme::Surface, theme::Hull, slot.left ? 3.0F : 4.0F);
    widgets::swatch(target, assets, mode, {chip.position + sf::Vector2f{8.0F, 8.0F}, {56.0F, 48.0F}},
        static_cast<std::uint8_t>(player), slot.present && !slot.down);
    const sf::Vector2f column = chip.position + sf::Vector2f{76.0F, 10.0F};
    text(target, assets.bodyBold, name, 15, theme::Text, column);
    if (slot.left) {
        text(target, assets.body, "Disconnected", 14, theme::Muted,
            column + sf::Vector2f{0.0F, 26.0F});
        return;
    }
    if (slot.down) {
        text(target, assets.bodyBold, "Down", 14, theme::Red, column + sf::Vector2f{0.0F, 26.0F});
        return;
    }
    text(target, assets.bodyBold, "HP", 14, theme::Text, column + sf::Vector2f{0.0F, 26.0F});
    const sf::Color filled = points == 1 ? theme::Red : theme::Green;
    for (std::uint8_t pip = 0; pip < 3; ++pip) {
        box(target,
            {column + sf::Vector2f{26.0F + (static_cast<float>(pip) * 26.0F), 30.0F}, {22.0F, 12.0F}},
            pip >= points ? sf::Color::White : filled, theme::Hull, 2.0F);
    }
}

/// Top left, from `corner`: team score and level. Returns where they end on
/// the right.
float drawRunStats(
    sf::RenderTarget& target, const Assets& assets, const Session& session, sf::Vector2f corner)
{
    const std::string score = formatScore(session.score);
    const float scoreWidth = std::max(textWidth(assets.bodyBold, "TEAM SCORE", 13),
                                 textWidth(assets.bodyBold, score, 32))
        + 36.0F;
    box(target, {corner, {scoreWidth, 64.0F}}, theme::Surface, theme::Hull, 4.0F);
    text(target, assets.bodyBold, "TEAM SCORE", 13, theme::Muted,
        corner + sf::Vector2f{18.0F, 9.0F});
    text(target, assets.bodyBold, score, 32, theme::Blue, corner + sf::Vector2f{18.0F, 28.0F});

    const std::string level = std::to_string(session.level);
    const sf::Vector2f levelAt = corner + sf::Vector2f{scoreWidth + 12.0F, 0.0F};
    const float levelWidth
        = std::max(textWidth(assets.bodyBold, "LEVEL", 13), textWidth(assets.display, level, 32))
        + 36.0F;
    box(target, {levelAt, {levelWidth, 64.0F}}, theme::Surface, theme::Hull, 4.0F);
    text(target, assets.bodyBold, "LEVEL", 13, theme::Muted, levelAt + sf::Vector2f{18.0F, 9.0F});
    text(target, assets.display, level, 32, theme::Text, levelAt + sf::Vector2f{18.0F, 28.0F});
    return levelAt.x + levelWidth;
}

/// Width of the box of the team's lives.
float livesWidth(const Assets& assets, const Session& session)
{
    constexpr float icon = 34.0F; // One small ship per life
    return std::max(textWidth(assets.bodyBold, "LIVES", 13),
               static_cast<float>(std::max<int>(session.lives, 1)) * icon)
        + 36.0F;
}

/// The team's lives as small ships, in `rect`.
void drawLives(sf::RenderTarget& target, const Assets& assets, ColourMode mode,
    const Session& session, sf::FloatRect rect)
{
    constexpr float icon = 34.0F;
    box(target, rect, theme::Surface, theme::Hull, 4.0F);
    text(target, assets.bodyBold, "LIVES", 13, theme::Muted,
        rect.position + sf::Vector2f{18.0F, 9.0F});
    const std::uint8_t colour = session.playerId.value_or(0);
    for (std::uint8_t life = 0; life < session.lives; ++life) {
        widgets::ship(target, assets, mode,
            {rect.position + sf::Vector2f{18.0F + (static_cast<float>(life) * icon), 32.0F},
                {30.0F, 16.0F}},
            colour);
    }
}

/// The boss on screen, if any: its health and full health (from the snapshots).
std::optional<std::pair<std::uint8_t, std::uint8_t>> bossHealth(ecs::World& world)
{
    std::optional<std::pair<std::uint8_t, std::uint8_t>> found;
    world.query<NetEntity>([&](ecs::Entity /*entity*/, const NetEntity& data) {
        if (data.kind == network::protocol::EntityKind::Boss) {
            found = std::pair{data.health, data.peakHealth};
        }
    });
    return found;
}

/// The panel of the boss bar: centred at the top of `area`, between the
/// boxes ending at `left` and starting at `right` on the first row. If they
/// leave too little room, it goes at `below`, still centred.
sf::FloatRect bossPanel(sf::FloatRect area, float top, float left, float right, float below)
{
    constexpr float widest = 456.0F;
    constexpr float narrowest = 260.0F;
    constexpr float gap = 12.0F;
    const float centre = area.position.x + (area.size.x / 2.0F);
    const float room = 2.0F * (std::min(centre - left, right - centre) - gap);
    if (room >= narrowest) {
        const float width = std::min(widest, room);
        return {{centre - (width / 2.0F), top}, {width, 64.0F}};
    }
    return {{centre - (widest / 2.0F), below}, {widest, 64.0F}};
}

/// The distance to the next boss, or the boss's health while it is there.
void drawBossBar(sf::RenderTarget& target, const Assets& assets, ecs::World& world,
    const Session& session, sf::FloatRect panel)
{
    const sf::FloatRect bar{
        panel.position + sf::Vector2f{18.0F, 34.0F}, {panel.size.x - 36.0F, 16.0F}};
    const auto boss = bossHealth(world);
    float filled = static_cast<float>(session.bossProgress) / 100.0F;
    std::string label = "NEXT BOSS  " + std::to_string(session.bossProgress) + " %";
    sf::Color colour = theme::Blue;
    if (boss) {
        const auto [health, full] = *boss;
        filled = full > 0 ? static_cast<float>(health) / static_cast<float>(full) : 1.0F;
        label = "BOSS";
        colour = theme::Red;
    } else if (session.bossProgress >= 100) {
        label = "BOSS INCOMING";
        colour = theme::Red;
    }
    box(target, panel, theme::Surface, theme::Hull, 4.0F);
    text(target, assets.bodyBold, label, 13, theme::Muted,
        panel.position + sf::Vector2f{18.0F, 9.0F});
    box(target, bar, sf::Color::White, theme::Hull, 2.0F);
    if (filled > 0.0F) {
        box(target, {bar.position, {bar.size.x * std::min(filled, 1.0F), bar.size.y}}, colour,
            theme::Hull, 2.0F);
    }
}

void drawHud(sf::RenderTarget& target, const Assets& assets, ecs::World& world,
    const Session& session, float now, sf::FloatRect area)
{
    constexpr float margin = 20.0F;
    constexpr float gap = 12.0F;
    constexpr float rowHeight = 76.0F;
    const float left = area.position.x + margin;
    const float rightEdge = area.position.x + area.size.x - margin;
    const float firstRow = area.position.y + margin;

    // Team score and level, top left
    const float statsEnd = drawRunStats(target, assets, session, {left, firstRow});

    // Top right, from the right: one chip per player, then the team's lives.
    // On a new row when they would reach the score, or the left of the screen
    const auto health = shipHealth(world);
    const Settings& settings = singleton<Settings>(world);
    float right = rightEdge;
    float row = firstRow;
    float firstRowStart = rightEdge; // Where the boxes of the first row start
    const auto place = [&](float width) {
        if (right - width < (row == firstRow ? statsEnd + gap : left)) {
            right = rightEdge;
            row += rowHeight;
        }
        const sf::FloatRect rect{{right - width, row}, {width, 64.0F}};
        right -= width + gap;
        if (row == firstRow) {
            firstRowStart = rect.position.x;
        }
        return rect;
    };
    for (std::size_t back = session.players.size(); back > 0; --back) {
        const std::size_t i = back - 1;
        const PlayerSlot& slot = session.players.at(i);
        if (!slot.present && !slot.left) {
            continue;
        }
        const std::string name = session.playerId == i ? slot.name + " (you)" : slot.name;
        const float width
            = 8.0F + 56.0F + 12.0F + std::max(104.0F, textWidth(assets.bodyBold, name, 15)) + 14.0F;
        drawPlayerChip(
            target, assets, settings.colourMode, place(width), i, slot, name, health.at(i));
    }
    drawLives(target, assets, settings.colourMode, session, place(livesWidth(assets, session)));

    // Centred at the top: the distance to the next boss, then its health
    const sf::FloatRect panel = bossPanel(area, firstRow, statsEnd, firstRowStart, row + rowHeight);
    drawBossBar(target, assets, world, session, panel);

    if (!session.notice.empty() && now < session.noticeUntil) {
        const float width = textWidth(assets.bodyBold, session.notice, 16) + 28.0F;
        const float below = std::max(row, panel.position.y) + rowHeight + 8.0F;
        const sf::FloatRect toast{{rightEdge - width, below}, {width, 40.0F}};
        box(target, toast, theme::Blue, theme::LightBlue, 3.0F);
        text(target, assets.bodyBold, session.notice, 16, sf::Color::White,
            toast.position + sf::Vector2f{14.0F, 12.0F});
    }

    const std::string reminder = controlsReminder(settings);
    const float reminderWidth = textWidth(assets.body, reminder, 15) + 24.0F;
    const float bottom = area.position.y + area.size.y - margin;
    box(target, {{left, bottom - 32.0F}, {reminderWidth, 32.0F}}, theme::Surface, theme::Hull, 3.0F);
    text(target, assets.body, reminder, 15, theme::Text, {left + 12.0F, bottom - 23.0F});
}

void drawMenu(sf::RenderTarget& target, const Assets& assets, const Session& session, sf::FloatRect area)
{
    box(target, area, theme::Overlay, theme::Overlay, 0.0F);
    box(target, layout::MenuPanel, theme::Surface, theme::Hull, 4.0F, true);
    text(target, assets.display, "Menu", 32, theme::Text, {462.0F, 246.0F});
    text(target, assets.body, "The game keeps running for the other players.", 15, theme::Muted,
        {462.0F, 300.0F});
    widgets::button(target, assets, layout::Resume, "Resume", true,
        session.focus == layout::ResumeButton);
    widgets::button(target, assets, layout::MenuSettings, "Settings", false,
        session.focus == layout::MenuSettingsButton);
    widgets::button(target, assets, layout::LeaveGame, "Leave game", false,
        session.focus == layout::LeaveButton);
}

/// One settings row: label on the left, value on the right, focus ring.
void settingsRow(sf::RenderTarget& target, const Assets& assets, int row, std::string_view label,
    const std::string& value, bool focused, bool waiting)
{
    const sf::FloatRect rect = layout::settingsRow(row);
    box(target, rect, theme::Surface, focused ? theme::Blue : theme::HullLight, focused ? 4.0F : 2.0F);
    text(target, assets.bodyBold, label, 17, theme::Text,
        {rect.position.x + 16.0F, rect.position.y + 12.0F});
    const unsigned size = 17;
    const float width = textWidth(assets.bodyBold, value, size);
    text(target, assets.bodyBold, value, size, waiting ? theme::Blue : theme::Text,
        {rect.position.x + rect.size.x - 16.0F - width, rect.position.y + 12.0F});
}

void drawSettings(sf::RenderTarget& target, const Assets& assets, const Session& session,
    const Settings& settings, sf::FloatRect area)
{
    box(target, area, theme::Overlay, theme::Overlay, 0.0F);
    box(target, layout::SettingsPanel, theme::Surface, theme::Hull, 4.0F, true);
    text(target, assets.display, "Settings", 32, theme::Text, {245.0F, 134.0F});

    settingsRow(target, assets, layout::VolumeRow, "Sound effects",
        settings.volume == 0 ? "< Muted >" : "< " + std::to_string(settings.volume) + " % >",
        session.focus == layout::VolumeRow, false);

    settingsRow(target, assets, layout::ColourRow, "Colour-blind mode",
        "< " + std::string(colourModeName(settings.colourMode)) + " >",
        session.focus == layout::ColourRow, false);

    settingsRow(target, assets, layout::InterfaceSizeRow, "Interface size",
        "< " + std::to_string(settings.interfaceSize) + " % >",
        session.focus == layout::InterfaceSizeRow, false);

    constexpr std::array<std::string_view, ActionCount> actions{
        "Move up",
        "Move down",
        "Move left",
        "Move right",
        "Fire",
        "Move the Force",
    };
    for (std::size_t action = 0; action < ActionCount; ++action) {
        const int row = layout::FirstKeyRow + static_cast<int>(action);
        const bool waiting = session.capturing == static_cast<Action>(action);
        const auto button = settings.buttons.at(action);
        const std::string key = waiting
            ? "Key or button (Esc: cancel)"
            : "[ " + std::string(keyName(settings.keys.at(action)).value_or("?")) + " ]"
                + (button ? "  [ Pad " + std::to_string(*button) + " ]" : "");
        settingsRow(target, assets, row, actions.at(action), key, session.focus == row, waiting);
    }
    widgets::button(target, assets, layout::settingsRow(layout::ResetRow), "Reset controls", false,
        session.focus == layout::ResetRow);
    widgets::button(target, assets, layout::settingsRow(layout::BackRow), "Back", true,
        session.focus == layout::BackRow);

    if (!settings.saveError.empty()) {
        errorBox(target, assets, {{245.0F, 498.0F}, {790.0F, 40.0F}}, settings.saveError);
    }
    text(target, assets.body, "Arrows: move  ·  Enter: change  ·  Esc: back", 15, theme::Muted,
        {245.0F, 552.0F});
}

void drawGameOver(sf::RenderTarget& target, const Assets& assets, ColourMode mode,
    const Session& session, sf::FloatRect area, bool compact)
{
    drawLightBackground(target, area);
    const unsigned titleSize = compact ? 36U : 150U;
    const float titleWidth = textWidth(assets.display, "GAME OVER", titleSize);
    title(target, assets, "GAME OVER", titleSize,
        {(theme::UiWidth - titleWidth) / 2.0F, compact ? 205.0F : 60.0F});

    const std::string score = formatScore(session.score);
    const float boxWidth = std::max(textWidth(assets.bodyBold, score, 72), 160.0F) + 72.0F;
    const sf::FloatRect scoreBox{{(theme::UiWidth - boxWidth) / 2.0F, 248.0F}, {boxWidth, 130.0F}};
    box(target, scoreBox, theme::Surface, theme::Hull, 4.0F, true);
    centredText(target, assets.bodyBold, "TEAM SCORE", 13, theme::Muted,
        {scoreBox.position + sf::Vector2f{0.0F, 18.0F}, {boxWidth, 16.0F}});
    centredText(target, assets.bodyBold, score, 72, theme::Blue,
        {scoreBox.position + sf::Vector2f{0.0F, 42.0F}, {boxWidth, 72.0F}});
    centredText(target, assets.bodyBold, "Level " + std::to_string(session.level) + " reached", 17,
        theme::Text, {{0.0F, 384.0F}, {theme::UiWidth, 22.0F}});

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
    const float width = (GridWidth - (Gap * (count - 1.0F))) / count;
    for (std::size_t n = 0; n < crew.size(); ++n) {
        const std::size_t i = crew.at(n);
        const sf::FloatRect card{
            {270.0F + (static_cast<float>(n) * (width + Gap)), 412.0F}, {width, 160.0F}};
        crewCard(target, assets, mode, card, i, session.players.at(i), false);
    }
    widgets::button(target, assets, layout::PlayAgain, "Play again", true,
        session.focus == layout::PlayAgainButton);
    widgets::button(target, assets, layout::BackToMenu, "Back to menu", false,
        session.focus == layout::BackToMenuButton);
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
    const Settings& settings = singleton<Settings>(world);
    const ColourMode mode = settings.colourMode;
    const int size = settings.interfaceSize;
    const bool compact = layout::isCompact(size);
    const sf::FloatRect area = layout::visibleArea(session, size);
    _window.setView(letterboxed(area, _window.getSize()));
    switch (session.screen) {
    case Screen::Connect:
        drawConnect(_window, _assets, mode, session, area, compact);
        break;
    case Screen::WaitingRoom:
        drawWaitingRoom(_window, _assets, mode, session, area, size);
        break;
    case Screen::Game:
        drawHud(_window, _assets, world, session, time.elapsedSeconds(), area);
        if (session.menuOpen) {
            drawMenu(_window, _assets, session, area);
        }
        break;
    case Screen::GameOver:
        drawGameOver(_window, _assets, mode, session, area, compact);
        break;
    }
    if (session.settingsOpen) {
        drawSettings(_window, _assets, session, settings, area);
    }
    _window.display();
}

} // namespace client
