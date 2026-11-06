#include "client/Gamepad.hpp"

#include "network/Protocol/Protocol.hpp"

#include <SFML/Window/Joystick.hpp>

#include <algorithm>
#include <array>

namespace client::gamepad {

using namespace network::protocol;

std::uint8_t mask(const PadState& pad, const Settings& settings) noexcept
{
    const int horizontal = direction(pad.x) != 0 ? direction(pad.x) : direction(pad.povX);
    const int vertical = direction(pad.y) != 0 ? direction(pad.y) : direction(povY(pad.povY));
    unsigned bits = 0;
    bits |= horizontal < 0 ? input::Left : 0U;
    bits |= horizontal > 0 ? input::Right : 0U;
    bits |= vertical < 0 ? input::Up : 0U;
    bits |= vertical > 0 ? input::Down : 0U;

    constexpr std::array<std::uint8_t, ActionCount> actionBits{
        input::Up, input::Down, input::Left, input::Right, input::Fire};
    for (std::size_t action = 0; action < ActionCount; ++action) {
        const auto button = settings.buttons.at(action);
        if (button && *button < MaxButtons && ((pad.buttons >> *button) & 1U) != 0) {
            bits |= actionBits.at(action);
        }
    }
    return static_cast<std::uint8_t>(bits);
}

PadState read(unsigned joystick)
{
    if (!sf::Joystick::isConnected(joystick)) {
        return {};
    }
    using sf::Joystick::Axis;
    PadState pad{
        .x = sf::Joystick::getAxisPosition(joystick, Axis::X),
        .y = sf::Joystick::getAxisPosition(joystick, Axis::Y),
        .povX = sf::Joystick::getAxisPosition(joystick, Axis::PovX),
        .povY = sf::Joystick::getAxisPosition(joystick, Axis::PovY),
    };
    const unsigned count = std::min(sf::Joystick::getButtonCount(joystick), MaxButtons);
    for (unsigned button = 0; button < count; ++button) {
        if (sf::Joystick::isButtonPressed(joystick, button)) {
            pad.buttons |= 1U << button;
        }
    }
    return pad;
}

bool anyConnected()
{
    for (unsigned joystick = 0; joystick < sf::Joystick::Count; ++joystick) {
        if (sf::Joystick::isConnected(joystick)) {
            return true;
        }
    }
    return false;
}

} // namespace client::gamepad
