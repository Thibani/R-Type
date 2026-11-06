#include "client/Systems/InputSystem.hpp"

#include "client/Components/Controls.hpp"
#include "client/Components/Session.hpp"
#include "client/Components/Settings.hpp"
#include "client/Gamepad.hpp"
#include "client/Settings.hpp"
#include "client/Singleton.hpp"
#include "client/Ui/Layout.hpp"
#include "client/Ui/View.hpp"
#include "ecs/Loop/GameLoop.hpp"
#include "ecs/World/World.hpp"
#include "network/Protocol/PacketCodec.hpp"

#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

namespace client {

using namespace network::protocol;

namespace {

constexpr std::size_t MaxAddressLength = 63;
constexpr std::size_t MaxPortLength = 5;

void appendUtf8(std::string& text, char32_t c)
{
    const auto byte
        = [](char32_t value) { return static_cast<char>(static_cast<std::uint8_t>(value)); };
    if (c < 0x80) {
        text += byte(c);
    } else if (c < 0x800) {
        text += byte(0xC0U | (c >> 6U));
        text += byte(0x80U | (c & 0x3FU));
    } else if (c < 0x10000) {
        text += byte(0xE0U | (c >> 12U));
        text += byte(0x80U | ((c >> 6U) & 0x3FU));
        text += byte(0x80U | (c & 0x3FU));
    } else {
        text += byte(0xF0U | (c >> 18U));
        text += byte(0x80U | ((c >> 12U) & 0x3FU));
        text += byte(0x80U | ((c >> 6U) & 0x3FU));
        text += byte(0x80U | (c & 0x3FU));
    }
}

/// Removes the last character (one to four UTF-8 bytes).
void eraseLast(std::string& text)
{
    while (!text.empty() && (static_cast<std::uint8_t>(text.back()) & 0xC0U) == 0x80U) {
        text.pop_back(); // Continuation bytes
    }
    if (!text.empty()) {
        text.pop_back();
    }
}

std::string* focusedField(Session& session)
{
    if (session.screen != Screen::Connect || session.settingsOpen) {
        return nullptr;
    }
    switch (session.focus) {
    case layout::NameField:
        return &session.name;
    case layout::PasswordField:
        return &session.password;
    case layout::AddressField:
        return &session.address;
    case layout::PortField:
        return &session.port;
    default:
        return nullptr;
    }
}

/// Adds a typed character if the focused field accepts it.
void type(Session& session, char32_t c)
{
    std::string* field = focusedField(session);
    if (field == nullptr || c < 0x20 || c == 0x7F) {
        return; // Control characters (Enter, Tab, Backspace) are keys, not text
    }
    if (session.focus == layout::NameField) {
        std::string longer = *field;
        appendUtf8(longer, c);
        if (isValidName(longer)) { // At most 16 bytes of UTF-8 (protocol §2.2)
            *field = longer;
        }
    } else if (session.focus == layout::PasswordField) {
        std::string longer = *field;
        appendUtf8(longer, c);
        if (longer.size() <= MaxPasswordSize) {
            *field = longer;
        }
    } else if (session.focus == layout::AddressField) {
        if (c < 0x80 && field->size() < MaxAddressLength) {
            appendUtf8(*field, c);
        }
    } else if (c >= U'0' && c <= U'9' && field->size() < MaxPortLength) {
        appendUtf8(*field, c);
    }
}

void openSettings(Session& session)
{
    session.settingsOpen = true;
    session.capturing.reset();
    session.focus = layout::VolumeRow;
}

/// Back to the screen the settings were opened from, focus on its button.
void closeSettings(Session& session)
{
    session.settingsOpen = false;
    session.capturing.reset();
    session.focus
        = session.screen == Screen::Connect ? layout::SettingsButton : layout::MenuSettingsButton;
}

void changeColourMode(Settings& settings, int step)
{
    constexpr int count = static_cast<int>(ColourModeCount);
    settings.colourMode
        = static_cast<ColourMode>((static_cast<int>(settings.colourMode) + step + count) % count);
    settings.dirty = true;
}

void changeInterfaceSize(Settings& settings, int step)
{
    const auto& sizes = Settings::InterfaceSizes;
    const auto count = std::ssize(sizes);
    const auto current = std::ranges::find(sizes, settings.interfaceSize) - sizes.begin();
    settings.interfaceSize = sizes.at(static_cast<std::size_t>((current + step + count) % count));
    settings.dirty = true;
}

/// Arrows, stick or D-pad in a menu (`dx`, `dy`: one of them 0, the other -1
/// or 1): the focus moves to the nearest control that way. Values change with
/// Enter, so Left and Right always move, even on a setting row.
void navigate(Session& session, const Settings& settings, int dx, int dy)
{
    session.focus = layout::neighbour(
        layout::focusables(session, settings.interfaceSize), session.focus, dx, dy);
}

/// A click on the volume sets it where the track was clicked, and the left half
/// of a selector goes back. False for any other click.
bool clickSettings(const Session& session, Settings& settings, sf::Vector2f point)
{
    if (!session.settingsOpen) {
        return false;
    }
    if (session.focus == layout::VolumeRow) {
        const float ratio = std::clamp(
            (point.x - layout::VolumeTrack.position.x) / layout::VolumeTrack.size.x, 0.0F, 1.0F);
        const float steps = ratio * static_cast<float>(Settings::MaxVolume / Settings::VolumeStep);
        settings.volume = static_cast<int>(std::lround(steps)) * Settings::VolumeStep;
        settings.dirty = true;
        return true;
    }
    if (session.focus == layout::ColourRow && point.x < layout::ColourSelector.getCenter().x) {
        changeColourMode(settings, -1);
        return true;
    }
    if (session.focus == layout::InterfaceSizeRow
        && point.x < layout::InterfaceSizeSelector.getCenter().x) {
        changeInterfaceSize(settings, -1);
        return true;
    }
    return false;
}

/// Does what the focused (or clicked) row of the settings does.
void activateSettings(Session& session, Settings& settings, int index)
{
    if (index == layout::VolumeRow) {
        // Enter, A or a click raises it and wraps to muted
        settings.volume
            = settings.volume >= Settings::MaxVolume ? 0 : settings.volume + Settings::VolumeStep;
        settings.dirty = true;
    } else if (index == layout::ColourRow) {
        changeColourMode(settings, 1);
    } else if (index == layout::InterfaceSizeRow) {
        changeInterfaceSize(settings, 1);
    } else if (index < layout::ResetRow) {
        session.capturing
            = static_cast<Action>(index - layout::FirstKeyRow); // Next key pressed is bound
    } else if (index == layout::ResetRow) {
        settings.keys = Settings::DefaultKeys;
        settings.buttons = Settings::DefaultButtons;
        settings.dirty = true;
    } else {
        closeSettings(session);
    }
}

/// Does what the focused (or clicked) element of the screen does.
void activate(Session& session, Settings& settings, int index, ecs::GameLoop& loop)
{
    if (session.settingsOpen) {
        activateSettings(session, settings, index);
        return;
    }
    switch (session.screen) {
    case Screen::Connect:
        if (index == layout::QuitButton) {
            session.request = Request::Quit;
            loop.stop();
        } else if (index == layout::LoginButton) {
            session.request = Request::Login;
        } else if (index == layout::CreateAccountButton) {
            session.request = Request::CreateAccount;
        } else if (index == layout::SettingsButton) {
            openSettings(session);
        } else {
            session.focus = index; // A field: just focus it
        }
        break;
    case Screen::WaitingRoom:
    case Screen::GameOver:
        session.request = Request::Leave;
        break;
    case Screen::Game:
        if (index == layout::ResumeButton) {
            session.menuOpen = false;
        } else if (index == layout::MenuSettingsButton) {
            openSettings(session);
        } else {
            session.request = Request::Leave;
        }
        break;
    }
}

void moveFocus(Session& session, const Settings& settings, int step)
{
    const int count = static_cast<int>(layout::focusables(session, settings.interfaceSize).size());
    if (count > 0) {
        session.focus = (session.focus + step + count) % count;
    }
}

/// While a settings row waits for a key: the key pressed becomes the binding.
/// Esc cancels; a key that cannot be bound is ignored and the row keeps waiting.
void capture(Session& session, Settings& settings, Action action, sf::Keyboard::Key key)
{
    if (key == sf::Keyboard::Key::Escape) {
        session.capturing.reset();
    } else if (bindKey(settings, action, key)) {
        settings.dirty = true;
        session.capturing.reset();
    }
}

/// While a settings row waits: the pad button pressed becomes the binding.
/// Start cancels, like Esc.
void capturePad(Session& session, Settings& settings, Action action, unsigned button)
{
    if (button == gamepad::StartButton) {
        session.capturing.reset();
    } else if (bindButton(settings, action, button)) {
        settings.dirty = true;
        session.capturing.reset();
    }
}

/// Esc: closes the settings or the menu, leaves the screen, or quits.
void escape(Session& session, ecs::GameLoop& loop)
{
    if (session.settingsOpen) {
        closeSettings(session);
    } else if (session.screen == Screen::Connect) {
        session.request = Request::Quit;
        loop.stop();
    } else if (session.screen == Screen::Game) {
        session.menuOpen = !session.menuOpen;
        session.focus = 0;
    } else {
        session.request = Request::Leave;
    }
}

/// Enter, or A on a pad: activates the focus. In the form, logs in from any field.
void confirm(Session& session, Settings& settings, ecs::GameLoop& loop)
{
    activate(session, settings,
        !session.settingsOpen && session.screen == Screen::Connect
                && session.focus < layout::LoginButton
            ? layout::LoginButton
            : session.focus,
        loop);
}

bool inMenu(const Session& session) noexcept
{
    return session.screen != Screen::Game || session.menuOpen || session.settingsOpen;
}

void onPadButton(Session& session, Settings& settings, unsigned button, ecs::GameLoop& loop)
{
    if (session.capturing) {
        capturePad(session, settings, *session.capturing, button);
        return;
    }
    // Start opens and closes the menu; B goes back. A pad never quits the game
    const bool back
        = button == gamepad::StartButton || (button == gamepad::BackButton && inMenu(session));
    if (back && (session.screen != Screen::Connect || session.settingsOpen)) {
        escape(session, loop);
    } else if (button == gamepad::AcceptButton && inMenu(session)) {
        confirm(session, settings, loop);
    }
}

/// A stick or D-pad pushed in a menu: moves the focus, or adjusts a setting.
void onPadPush(Session& session, const Settings& settings, bool vertical, int direction)
{
    if (direction == 0 || !inMenu(session) || session.capturing) {
        return;
    }
    navigate(session, settings, vertical ? 0 : direction, vertical ? direction : 0);
}

void onKey(
    Session& session, Settings& settings, const sf::Event::KeyPressed& key, ecs::GameLoop& loop)
{
    using sf::Keyboard::Key;
    if (session.capturing) {
        capture(session, settings, *session.capturing, key.code);
        return;
    }
    const bool menu = inMenu(session);
    switch (key.code) {
    case Key::Escape:
        escape(session, loop);
        break;
    case Key::Tab:
        moveFocus(session, settings, key.shift ? -1 : 1);
        break;
    case Key::Down:
    case Key::Up:
        if (menu) { // In the game, arrows move the ship
            navigate(session, settings, 0, key.code == Key::Down ? 1 : -1);
        }
        break;
    case Key::Left:
    case Key::Right:
        if (menu) {
            navigate(session, settings, key.code == Key::Right ? 1 : -1, 0);
        }
        break;
    case Key::Enter:
        if (menu) {
            confirm(session, settings, loop);
        }
        break;
    case Key::Backspace:
        if (std::string* field = focusedField(session)) {
            eraseLast(*field);
        }
        break;
    default:
        break;
    }
}

/// The INPUT mask of the keys held right now, with the player's bindings.
std::uint8_t heldKeys(const Settings& settings)
{
    std::uint8_t mask = 0;
    const auto add = [&](Action action, std::uint8_t bit) {
        if (sf::Keyboard::isKeyPressed(settings.keys.at(static_cast<std::size_t>(action)))) {
            mask = static_cast<std::uint8_t>(mask | bit);
        }
    };
    add(Action::Up, input::Up);
    add(Action::Down, input::Down);
    add(Action::Left, input::Left);
    add(Action::Right, input::Right);
    add(Action::Fire, input::Fire);
    return mask;
}

} // namespace

InputSystem::InputSystem(sf::RenderWindow& window, ecs::GameLoop& loop) noexcept
    : _window(window)
    , _loop(loop)
{
}

void InputSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    auto& session = singleton<Session>(world);
    auto& settings = singleton<Settings>(world);
    while (const std::optional event = _window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            session.request = Request::Quit;
            _loop.stop();
        } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
            onKey(session, settings, *key, _loop);
        } else if (const auto* pressed = event->getIf<sf::Event::JoystickButtonPressed>()) {
            onPadButton(session, settings, pressed->button, _loop);
        } else if (const auto* moved = event->getIf<sf::Event::JoystickMoved>()) {
            onPadMoved(session, settings, *moved);
        } else if (const auto* typed = event->getIf<sf::Event::TextEntered>()) {
            type(session, typed->unicode);
        } else if (const auto* click = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (click->button != sf::Mouse::Button::Left || session.capturing) {
                continue;
            }
            const sf::Vector2f point = _window.mapPixelToCoords(click->position,
                letterboxed(
                    layout::visibleArea(session, settings.interfaceSize), _window.getSize()));
            const auto targets = layout::focusables(session, settings.interfaceSize);
            for (std::size_t i = 0; i < targets.size(); ++i) {
                if (targets.at(i).contains(point)) {
                    session.focus = static_cast<int>(i);
                    if (!clickSettings(session, settings, point)) {
                        activate(session, settings, session.focus, _loop);
                    }
                    break;
                }
            }
        }
    }
    // Held keys and pads only steer the ship in the game itself, never through a menu
    std::uint8_t mask = 0;
    if (!inMenu(session) && _window.hasFocus()) {
        mask = heldKeys(settings);
        for (unsigned joystick = 0; joystick < sf::Joystick::Count; ++joystick) {
            mask = static_cast<std::uint8_t>(
                mask | gamepad::mask(gamepad::read(joystick), settings));
        }
    }
    singleton<PlayerControls>(world).mask = mask;
}

void InputSystem::onPadMoved(
    Session& session, Settings& settings, const sf::Event::JoystickMoved& moved)
{
    using sf::Joystick::Axis;
    std::size_t slot = 0;
    float position = moved.position;
    switch (moved.axis) {
    case Axis::X:
        slot = 0;
        break;
    case Axis::Y:
        slot = 1;
        break;
    case Axis::PovX:
        slot = 2;
        break;
    case Axis::PovY:
        slot = 3;
        position = gamepad::povY(position);
        break;
    default:
        return; // Triggers and the second stick do nothing
    }
    if (moved.joystickId >= _padDirections.size()) {
        return;
    }
    int& last = _padDirections.at(moved.joystickId).at(slot);
    const int direction = gamepad::direction(position);
    if (direction != last) {
        last = direction;
        onPadPush(session, settings, slot % 2 == 1, direction);
    }
}

} // namespace client
