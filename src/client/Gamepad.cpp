#include "client/Gamepad.hpp"

#include "network/Protocol/Protocol.hpp"

#include <SFML/Window/Joystick.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

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
        input::Up, input::Down, input::Left, input::Right, input::Fire, input::Pod};
    for (std::size_t action = 0; action < ActionCount; ++action) {
        const auto button = settings.buttons.at(action);
        if (button && *button < MaxButtons && ((pad.buttons >> *button) & 1U) != 0) {
            bits |= actionBits.at(action);
        }
    }
    return static_cast<std::uint8_t>(bits);
}

bool isMouseName(std::string_view name) noexcept
{
    constexpr std::string_view Mouse = "mouse";
    const auto lower
        = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
    for (std::size_t start = 0; start + Mouse.size() <= name.size(); ++start) {
        bool same = true;
        for (std::size_t i = 0; i < Mouse.size() && same; ++i) {
            same = lower(name[start + i]) == Mouse[i];
        }
        if (same) {
            return true;
        }
    }
    return false;
}

bool isPad(unsigned joystick)
{
    return sf::Joystick::isConnected(joystick)
        && !isMouseName(sf::Joystick::getIdentification(joystick).name.toAnsiString());
}

PadState read(unsigned joystick)
{
    if (!isPad(joystick)) {
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
        if (isPad(joystick)) {
            return true;
        }
    }
    return false;
}

} // namespace client::gamepad
