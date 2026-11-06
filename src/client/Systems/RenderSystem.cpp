#include "client/Systems/RenderSystem.hpp"

#include "client/Assets.hpp"
#include "client/Components/Appearance.hpp"
#include "client/Components/Session.hpp"
#include "client/Components/Settings.hpp"
#include "client/Singleton.hpp"
#include "client/Ui/Theme.hpp"
#include "client/Ui/View.hpp"
#include "client/Ui/Widgets.hpp"
#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Components/Motion.hpp"

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>

#include <cstdint>
#include <string>

namespace client {

namespace {

constexpr float PlaceholderSize = 56.0F;

void drawEntity(sf::RenderTarget& target, const Assets& assets, const Settings& settings,
    const game::Position& position, const Appearance& appearance)
{
    const sf::Vector2f centre{position.x, position.y};
    const auto frame = spriteFrame(assets, settings.colourMode, appearance.look, appearance.player);
    if (!frame) {
        // §5.2: a kind this client does not know is shown, never a crash
        const sf::Vector2f half{PlaceholderSize / 2.0F, PlaceholderSize / 2.0F};
        const sf::FloatRect rect{centre - half, {PlaceholderSize, PlaceholderSize}};
        widgets::box(target, rect, theme::Surface, theme::Hull, 3.0F);
        widgets::centredText(target, assets.display, "?", 30, theme::Text, rect);
        return;
    }
    sf::Sprite sprite(*frame->texture, frame->rect);
    sprite.setOrigin(sf::Vector2f(frame->rect.size) / 2.0F);
    sprite.setScale({frame->scale, frame->scale});
    sprite.setPosition(centre);
    target.draw(sprite);

    if (appearance.look == Look::PlayerShip && appearance.player < theme::Players.size()) {
        // The tag is interface: it follows the interface size, not the arena
        const unsigned size = 20U * static_cast<unsigned>(settings.interfaceSize) / 100U;
        const float above = (static_cast<float>(frame->rect.size.y) * frame->scale / 2.0F)
            + static_cast<float>(size) + 2.0F;
        widgets::tag(target, assets, "P" + std::to_string(appearance.player + 1),
            centre - sf::Vector2f{0.0F, above}, size);
    }
}

} // namespace

RenderSystem::RenderSystem(sf::RenderWindow& window, const Assets& assets) noexcept
    : _window(window)
    , _assets(assets)
{
}

void RenderSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    _window.clear(theme::Letterbox);
    _window.setView(letterboxed({game::arena::Width, game::arena::Height}, _window.getSize()));

    // Every screen has the moving sky. The interface is drawn over it
    sf::RectangleShape space({game::arena::Width, game::arena::Height});
    space.setFillColor(theme::GameBackground);
    _window.draw(space);

    sf::RectangleShape dot;
    world.query<Star, game::Position>(
        [&](ecs::Entity /*entity*/, Star& star, game::Position& position) {
            dot.setSize({star.size, star.size});
            dot.setPosition({position.x, position.y});
            // Far (small) stars are dimmer: a simple depth cue
            dot.setFillColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(60 * star.size)));
            _window.draw(dot);
        });

    if (singleton<Session>(world).screen != Screen::Game) {
        // Not during the game itself: nothing there may look like a shot
        sf::RectangleShape line;
        line.setFillColor(theme::Streak);
        line.setRotation(sf::degrees(-45.0F));
        world.query<Streak, game::Position>(
            [&](ecs::Entity /*entity*/, Streak& streak, game::Position& position) {
                line.setSize({streak.length, 2.0F});
                line.setPosition({position.x, position.y});
                _window.draw(line);
            });
        return; // The other screens are pure interface
    }

    const Settings& settings = singleton<Settings>(world);
    world.query<game::Position, Appearance>(
        [&](ecs::Entity /*entity*/, game::Position& position, Appearance& appearance) {
            drawEntity(_window, _assets, settings, position, appearance);
        });
}

} // namespace client
