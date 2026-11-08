#pragma once

#include "client/Components/Settings.hpp"

#include <cstdint>
#include <string_view>

/// Gamepads, through SFML's joystick API: what a pad does in the game and in
/// the menus. Everything but `read()` and `anyConnected()` is plain logic, so
/// the tests need no pad.
///
/// Button numbers are SFML's, the same on Linux and Windows for common pads
/// (Xbox layout): 0 A, 1 B, 2 X, 3 Y, 6 Back, 7 Start.
namespace client::gamepad {

inline constexpr unsigned AcceptButton = 0; // Menus: activate, like Enter
inline constexpr unsigned BackButton = 1; // Menus: back, like Esc
inline constexpr unsigned StartButton = 7; // Opens the menu. Never bound, like Esc
inline constexpr unsigned MaxButtons = 32; // sf::Joystick::ButtonCount
inline constexpr float DeadZone = 50.0F; // Of 100: a resting stick never moves the ship

/// One pad at one moment. Axes from -100 to 100, up and left negative.
struct PadState {
    float x{};
    float y{};
    float povX{}; // D-pad
    float povY{};
    std::uint32_t buttons{}; // Bit i: button i held
};

/// -1, 0 or 1: which way an axis points, past the dead zone.
[[nodiscard]]
constexpr int direction(float position) noexcept
{
    if (position <= -DeadZone) {
        return -1;
    }
    return position >= DeadZone ? 1 : 0;
}

/// The D-pad's vertical axis with up negative, like the stick. SFML reports it
/// the other way round on Windows.
[[nodiscard]]
constexpr float povY(float raw) noexcept
{
#ifdef _WIN32
    return -raw;
#else
    return raw;
#endif
}

/// The INPUT mask of a pad: stick and D-pad move, bound buttons do the rest.
[[nodiscard]]
std::uint8_t mask(const PadState& pad, const Settings& settings) noexcept;

/// True for a device named like a mouse. VirtualBox shows its mouse integration as a
/// joystick whose axes follow the pointer on the whole screen: read as a pad, it would hold
/// a direction down (left and down when the pointer is there) and cancel the arrow keys.
[[nodiscard]]
bool isMouseName(std::string_view name) noexcept;

/// True if `joystick` (0 to 7) is connected and is a pad, not a mouse seen as a joystick.
[[nodiscard]]
bool isPad(unsigned joystick);

/// The current state of pad `joystick` (0 to 7), or a resting pad if it is
/// not connected (or not a pad): plugging or unplugging one never stops anything.
[[nodiscard]]
PadState read(unsigned joystick);

/// True if at least one pad is connected (a mouse seen as a joystick does not count).
[[nodiscard]]
bool anyConnected();

} // namespace client::gamepad
