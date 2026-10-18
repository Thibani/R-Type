#include "client/Systems/InputSystem.hpp"

#include "client/Components/Controls.hpp"
#include "client/Components/Session.hpp"
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
        text += byte(0xC0 | (c >> 6));
        text += byte(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        text += byte(0xE0 | (c >> 12));
        text += byte(0x80 | ((c >> 6) & 0x3F));
        text += byte(0x80 | (c & 0x3F));
    } else {
        text += byte(0xF0 | (c >> 18));
        text += byte(0x80 | ((c >> 12) & 0x3F));
        text += byte(0x80 | ((c >> 6) & 0x3F));
        text += byte(0x80 | (c & 0x3F));
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
    if (session.screen != Screen::Connect) {
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

/// Does what the focused (or clicked) element of the screen does.
void activate(Session& session, int index, ecs::GameLoop& loop)
{
    switch (session.screen) {
    case Screen::Connect:
        if (index == layout::QuitButton) {
            session.request = Request::Quit;
            loop.stop();
        } else if (index == layout::ConnectButton) {
            session.request = Request::Connect;
        } else {
            session.focus = index; // A field: just focus it
        }
        break;
    case Screen::WaitingRoom:
    case Screen::GameOver:
        session.request = Request::Leave;
        break;
    case Screen::Game:
        if (index == 0) {
            session.menuOpen = false; // Resume
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

void onKey(Session& session, const sf::Event::KeyPressed& key, ecs::GameLoop& loop)
{
    using Key = sf::Keyboard::Key;
    const bool inMenu = session.screen != Screen::Game || session.menuOpen;
    switch (key.code) {
    case Key::Escape:
        if (session.screen == Screen::Connect) {
            session.request = Request::Quit;
            loop.stop();
        } else if (session.screen == Screen::Game) {
            session.menuOpen = !session.menuOpen;
            session.focus = 0;
        } else {
            session.request = Request::Leave;
        }
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
    case Key::Enter:
        if (!inMenu) {
            break;
        }
        // In the form, Enter connects from any field
        activate(session,
            session.screen == Screen::Connect && session.focus < layout::ConnectButton
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

std::uint8_t heldKeys()
{
    using sf::Keyboard::isKeyPressed;
    using Key = sf::Keyboard::Key;
    std::uint8_t mask = 0;
    const auto add = [&mask](Key key, std::uint8_t bit) {
        if (isKeyPressed(key)) {
            mask = static_cast<std::uint8_t>(mask | bit);
        }
    };
    add(Key::Up, input::Up);
    add(Key::Down, input::Down);
    add(Key::Left, input::Left);
    add(Key::Right, input::Right);
    add(Key::Space, input::Fire);
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
    Session& session = singleton<Session>(world);
    while (const std::optional event = _window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            session.request = Request::Quit;
            _loop.stop();
        } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
            onKey(session, *key, _loop);
        } else if (const auto* typed = event->getIf<sf::Event::TextEntered>()) {
            type(session, typed->unicode);
        } else if (const auto* click = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (click->button != sf::Mouse::Button::Left) {
                continue;
            }
            const sf::Vector2f point = _window.mapPixelToCoords(
                click->position, letterboxed({theme::UiWidth, theme::UiHeight}, _window.getSize()));
            const auto targets = layout::focusables(session);
            for (std::size_t i = 0; i < targets.size(); ++i) {
                if (targets.at(i).contains(point)) {
                    session.focus = static_cast<int>(i);
                    activate(session, session.focus, _loop);
                    break;
                }
            }
        }
    }
    // Held keys only count while the window has the focus
    singleton<PlayerControls>(world).mask = _window.hasFocus() ? heldKeys() : 0;
}

} // namespace client
