#pragma once

#include "ecs/System/System.hpp"

#include <SFML/Graphics/RenderWindow.hpp>

namespace ecs {
class GameLoop;
}

namespace client {

/// Frame start: reads the keyboard and the mouse.
///  - In the game: arrows + Space become the INPUT mask (`PlayerControls`),
///    Esc opens the menu.
///  - In the interface: typing, Tab / arrows to move the focus, Enter or a
///    click to activate; the result is a `Session::request` for the network.
/// Closing the window stops the loop.
class InputSystem final : public ecs::System {
public:
    InputSystem(sf::RenderWindow& window, ecs::GameLoop& loop) noexcept;

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    sf::RenderWindow& _window;
    ecs::GameLoop& _loop;
};

} // namespace client
