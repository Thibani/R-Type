#include "client/Systems/InputSystem.hpp"

#include "client/Components/Controls.hpp"
#include "client/Components/Session.hpp"
#include "client/Components/Settings.hpp"
#include "client/Settings.hpp"
#include "client/Singleton.hpp"
#include "client/Ui/Layout.hpp"
#include "client/Ui/Theme.hpp"
#include "client/Ui/View.hpp"
#include "ecs/Loop/GameLoop.hpp"
#include "ecs/World/World.hpp"
#include "network/Protocol/PacketCodec.hpp"

#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

#include <algorithm>
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
    session.focus = session.screen == Screen::Connect ? layout::SettingsButton : layout::MenuSettingsButton;
}

void changeVolume(Settings& settings, int step)
{
    settings.volume = std::clamp(settings.volume + step, 0, Settings::MaxVolume);
    settings.dirty = true;
}

/// Does what the focused (or clicked) row of the settings does.
void activateSettings(Session& session, Settings& settings, int index)
{
    if (index == layout::VolumeRow) {
        // A click or Enter raises it and wraps to muted; ← → step both ways
        settings.volume = settings.volume >= Settings::MaxVolume ? 0 : settings.volume + Settings::VolumeStep;
        settings.dirty = true;
    } else if (index < layout::ResetRow) {
        session.capturing = static_cast<Action>(index - layout::FirstKeyRow); // Next key pressed is bound
    } else if (index == layout::ResetRow) {
        settings.keys = Settings::DefaultKeys;
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
        } else if (index == layout::ConnectButton) {
            session.request = Request::Connect;
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

void moveFocus(Session& session, int step)
{
    const int count = static_cast<int>(layout::focusables(session).size());
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

void onKey(Session& session, Settings& settings, const sf::Event::KeyPressed& key, ecs::GameLoop& loop)
{
    using sf::Keyboard::Key;
    if (session.capturing) {
        capture(session, settings, *session.capturing, key.code);
        return;
    }
    const bool inMenu = session.screen != Screen::Game || session.menuOpen || session.settingsOpen;
    switch (key.code) {
    case Key::Escape:
        escape(session, loop);
        break;
    case Key::Tab:
        moveFocus(session, key.shift ? -1 : 1);
        break;
    case Key::Down:
    case Key::Up:
        if (inMenu) { // In the game, arrows move the ship
            moveFocus(session, key.code == Key::Down ? 1 : -1);
        }
        break;
    case Key::Left:
    case Key::Right:
        if (session.settingsOpen && session.focus == layout::VolumeRow) {
            changeVolume(settings, key.code == Key::Right ? Settings::VolumeStep : -Settings::VolumeStep);
        }
        break;
    case Key::Enter:
        if (!inMenu) {
            break;
        }
        // In the form, Enter connects from any field
        activate(session, settings,
            !session.settingsOpen && session.screen == Screen::Connect
                    && session.focus < layout::ConnectButton
                ? layout::ConnectButton
                : session.focus,
            loop);
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
        } else if (const auto* typed = event->getIf<sf::Event::TextEntered>()) {
            type(session, typed->unicode);
        } else if (const auto* click = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (click->button != sf::Mouse::Button::Left || session.capturing) {
                continue;
            }
            const sf::Vector2f point = _window.mapPixelToCoords(
                click->position, letterboxed({theme::UiWidth, theme::UiHeight}, _window.getSize()));
            const auto targets = layout::focusables(session);
            for (std::size_t i = 0; i < targets.size(); ++i) {
                if (targets.at(i).contains(point)) {
                    session.focus = static_cast<int>(i);
                    activate(session, settings, session.focus, _loop);
                    break;
                }
            }
        }
    }
    // Held keys only steer the ship in the game itself, never through a menu
    const bool playing = session.screen == Screen::Game && !session.menuOpen && !session.settingsOpen;
    singleton<PlayerControls>(world).mask = playing && _window.hasFocus() ? heldKeys(settings) : 0;
}

} // namespace client
