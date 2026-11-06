#pragma once

#include "ecs/System/System.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Joystick.hpp>

#include <array>

namespace ecs {
class GameLoop;
}

namespace client {

struct Session;
struct Settings;

/// Frame start: reads the keyboard and the mouse.
///  - In the game: arrows + Space become the INPUT mask (`PlayerControls`),
///    Esc opens the menu.
///  - In the interface: typing, Tab / arrows to move the focus, Enter or a
///    click to activate; the result is a `Session::request` for the network.
///  - Gamepads (`Gamepad.hpp`) too: stick or D-pad and bound buttons in the
///    game, Start for the menu; stick or D-pad, A and B in the menus.
/// Closing the window stops the loop.
class InputSystem final : public ecs::System {
public:
    InputSystem(sf::RenderWindow& window, ecs::GameLoop& loop) noexcept;

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    void onPadMoved(Session& session, Settings& settings, const sf::Event::JoystickMoved& moved);

    sf::RenderWindow& _window;
    ecs::GameLoop& _loop;
    // Where each pad's stick and D-pad pointed (X, Y, PovX, PovY): a menu
    // moves once when one is pushed, not every frame while it is held
    std::array<std::array<int, 4>, sf::Joystick::Count> _padDirections{};
};

} // namespace client
