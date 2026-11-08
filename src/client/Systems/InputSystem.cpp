#include "client/Systems/InputSystem.hpp"

#include "client/Chat.hpp"
#include "client/Components/Controls.hpp"
#include "client/Components/Session.hpp"
#include "client/Components/Settings.hpp"
#include "client/Gamepad.hpp"
#include "client/Replication.hpp"
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
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace client {

using namespace network::protocol;

namespace {

constexpr std::size_t MaxAddressLength = 63;
constexpr std::size_t MaxPortLength = 5;
// "/msg ", a name, a space, then a message of the longest size
constexpr std::size_t MaxChatInput = 5 + NameSize + 1 + MaxChatSize;

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
    if (session.settingsOpen || layout::leaderboardOpen(session)) {
        return nullptr;
    }
    if (session.screen == Screen::Home) {
        if (session.focus == layout::GroupCodeField) {
            return &session.groupCodeInput;
        }
        return session.focus == layout::FriendNameField ? &session.friendName : nullptr;
    }
    if (session.screen != Screen::Connect) {
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
    if (session.chatOpen) {
        if (c >= 0x20 && c != 0x7F) {
            std::string longer = session.chatInput;
            appendUtf8(longer, c);
            if (longer.size() <= MaxChatInput) {
                session.chatInput = longer;
            }
        }
        return;
    }
    std::string* field = focusedField(session);
    if (field == nullptr || c < 0x20 || c == 0x7F) {
        return; // Control characters (Enter, Tab, Backspace) are keys, not text
    }
    if (session.screen == Screen::Home && session.focus == layout::GroupCodeField) {
        // Upper case: a code is read aloud, and needs no thinking about it
        const char32_t upper = c >= U'a' && c <= U'z' ? c - U'a' + U'A' : c;
        if (upper < 0x80 && field->size() < GroupCodeSize
            && GroupCodeAlphabet.find(static_cast<char>(upper)) != std::string_view::npos) {
            appendUtf8(*field, upper);
        }
    } else if (session.screen == Screen::Home || session.focus == layout::NameField) {
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
    static_assert(layout::HomeSettingsButton == layout::MenuSettingsButton);
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

/// Asks for the name typed in the panel. Nothing if it is empty.
void askFriend(Session& session)
{
    if (!session.friendName.empty()) {
        session.friendMessage.clear();
        session.outgoing.emplace_back(FriendRequest{.name = session.friendName});
    }
}

/// Joins the group of the code typed. Says so if it cannot be a code.
void joinWithCode(Session& session)
{
    if (isValidGroupCode(session.groupCodeInput)) {
        session.outgoing.emplace_back(GroupJoin{.code = session.groupCodeInput});
        session.groupCodeInput.clear();
    } else {
        addChatLine(session,
            ChatLine{.kind = ChatLine::Kind::Notice,
                .name = {},
                .text = "A group code has 6 characters."});
    }
}

/// The big button: the leader starts the game, the others say whether they are ready.
void pressPlay(Session& session)
{
    if (leadsTheGroup(session)) {
        session.request = Request::Play;
        return;
    }
    const auto me = std::ranges::find(
        session.group.members, session.name, &network::protocol::GroupMemberState::name);
    const bool ready = me != session.group.members.end() && me->ready;
    session.outgoing.emplace_back(GroupReady{.ready = !ready});
}

/// Does what a button of the group's part does: `index` is its number on the main page.
void activateGroupButton(Session& session, int index)
{
    const auto member = [&](int first) -> const std::string& {
        return session.group.members.at(static_cast<std::size_t>(index - first)).name;
    };
    if (index == layout::NewGroupCodeButton) {
        session.outgoing.emplace_back(GroupCode{.action = GroupCodeAction::Generate});
    } else if (index == layout::DeleteGroupCodeButton) {
        session.outgoing.emplace_back(GroupCode{.action = GroupCodeAction::Delete});
    } else if (index == layout::GroupJoinButton) {
        joinWithCode(session);
    } else if (index == layout::LeaveGroupButton) {
        session.outgoing.emplace_back(GroupLeave{});
    } else if (index == layout::AcceptInvitationButton || index == layout::RefuseInvitationButton) {
        const auto reply
            = index == layout::AcceptInvitationButton ? FriendReply::Accept : FriendReply::Refuse;
        session.outgoing.emplace_back(
            GroupAnswer{.name = session.invitations.front(), .reply = reply});
        session.invitations.erase(session.invitations.begin());
    } else if (index >= layout::FirstPromoteButton) {
        session.outgoing.emplace_back(GroupPromote{.name = member(layout::FirstPromoteButton)});
    } else if (index >= layout::FirstKickButton) {
        session.outgoing.emplace_back(GroupKick{.name = member(layout::FirstKickButton)});
    }
}

/// Does what a button of a row of the friends panel does.
void activateFriendButton(Session& session, const layout::FriendButton& button)
{
    const std::string& name = session.friends.at(button.row).name;
    switch (button.action) {
    case layout::FriendAction::Accept:
        session.outgoing.emplace_back(FriendAnswer{.name = name, .reply = FriendReply::Accept});
        break;
    case layout::FriendAction::Refuse:
        session.outgoing.emplace_back(FriendAnswer{.name = name, .reply = FriendReply::Refuse});
        break;
    case layout::FriendAction::Remove:
        session.outgoing.emplace_back(FriendRemove{.name = name});
        break;
    }
}

/// Moves the rows of the panel; they cannot go past either end of the list.
void scrollFriends(Session& session, int interfaceSize, int step)
{
    const sf::FloatRect bar = layout::friendsBar(layout::visibleArea(session, interfaceSize), true);
    const std::size_t shown = layout::friendRowsShown(bar);
    const std::size_t last = session.friends.size() > shown ? session.friends.size() - shown : 0;
    if (step < 0) {
        session.friendScroll = session.friendScroll > 0 ? session.friendScroll - 1 : 0;
    } else {
        session.friendScroll = std::min(session.friendScroll + 1, last);
    }
}

/// Shows the ranking `board`, and asks the server for it: always fresh, never a copy
/// kept from before a game.
void showBoard(Session& session, network::protocol::Board board)
{
    session.board = board;
    session.leaderboard.reset();
    session.outgoing.emplace_back(network::protocol::LeaderboardRequest{.board = board});
}

void openLeaderboard(Session& session)
{
    session.leaderboardOpen = true;
    session.chatOpen = false;
    session.friendMenu.reset();
    session.focus
        = session.board == network::protocol::Board::Solo ? layout::SoloTab : layout::MultiTab;
    showBoard(session, session.board);
}

void closeLeaderboard(Session& session)
{
    session.leaderboardOpen = false;
    session.focus = layout::LeaderboardButton;
}

void openChat(Session& session, std::optional<std::string> target)
{
    session.chatOpen = true;
    session.chatInput.clear();
    session.chatTarget = std::move(target);
    session.chatScroll = 0;
}

void closeChat(Session& session)
{
    session.chatOpen = false;
    session.chatInput.clear();
    session.chatTarget.reset();
}

/// Sends what is typed, or says why it cannot be sent. The typing line disappears,
/// and what was said stays in the chat.
void submitChat(Session& session)
{
    const ChatOrder order = readChat(session.chatInput, session.chatTarget, session.friends);
    if (const auto* game = std::get_if<ChatOrder::ToGame>(&order.action)) {
        session.outgoing.emplace_back(ChatSend{.text = game->text});
    } else if (const auto* letter = std::get_if<ChatOrder::ToFriend>(&order.action)) {
        session.outgoing.emplace_back(PrivateSend{.name = letter->name, .text = letter->text});
    } else if (const auto* problem = std::get_if<ChatOrder::Problem>(&order.action)) {
        addChatLine(
            session, ChatLine{.kind = ChatLine::Kind::Notice, .name = {}, .text = problem->text});
    }
    closeChat(session);
}

/// Moves back in the chat, or towards its newest line.
void scrollChat(Session& session, int step)
{
    const std::size_t oldest = session.chat.empty() ? 0 : session.chat.size() - 1;
    if (step > 0) {
        session.chatScroll = std::min(session.chatScroll + 1, oldest);
    } else {
        session.chatScroll = session.chatScroll > 0 ? session.chatScroll - 1 : 0;
    }
}

/// Every key while typing in the chat: nothing reaches the ship or the screen behind.
void onChatKey(Session& session, sf::Keyboard::Key key)
{
    using sf::Keyboard::Key;
    if (key == Key::Escape) {
        closeChat(session);
    } else if (key == Key::Enter) {
        submitChat(session);
    } else if (key == Key::PageUp || key == Key::PageDown) {
        scrollChat(session, key == Key::PageUp ? 1 : -1);
    } else if (key == Key::Backspace) {
        if (!session.chatInput.empty()) {
            eraseLast(session.chatInput);
        } else {
            session.chatTarget.reset(); // Nothing left to erase: back to the group
        }
    }
}

/// A right click on a friend's row of the panel opens its menu. Anywhere else, closes it.
void rightClick(Session& session, sf::FloatRect area, sf::Vector2f point)
{
    using network::protocol::FriendState;
    session.friendMenu.reset();
    if (!layout::friendsOpen(session)) {
        return;
    }
    const sf::FloatRect bar = layout::friendsBar(area, true);
    const auto last
        = std::min(session.friends.size(), session.friendScroll + layout::friendRowsShown(bar));
    for (std::size_t row = session.friendScroll; row < last; ++row) {
        const FriendRow& person = session.friends.at(row);
        const bool isFriend
            = person.state == FriendState::Online || person.state == FriendState::Offline;
        if (isFriend && layout::friendRow(bar, row - session.friendScroll).contains(point)) {
            session.friendMenu = person.name;
        }
    }
}

/// What a left click does before reaching the parts of the screen: the menu's button, and
/// leaving the chat by clicking beside it. True if the click was used.
bool leftClickOnChatOrMenu(
    Session& session, const Settings& settings, sf::FloatRect area, sf::Vector2f point)
{
    const auto button = layout::friendMenuButton(session, layout::friendsBar(area, true));
    const auto invite = layout::friendInviteButton(session, layout::friendsBar(area, true));
    const std::optional<std::string> menu = std::exchange(session.friendMenu, std::nullopt);
    if (invite && menu && invite->contains(point)) {
        session.outgoing.emplace_back(GroupInvite{.name = *menu});
        return true;
    }
    if (button && button->contains(point)) {
        openChat(session, menu);
        session.focus = layout::PlayButton;
        session.friendsHovered = false;
        return true;
    }
    if (session.chatOpen) {
        if (layout::chatBox(session, area, settings.interfaceSize).contains(point)) {
            return true; // Still typing
        }
        closeChat(session);
    }
    return false;
}

/// Does what the focused (or clicked) element of the screen does.
void activate(Session& session, Settings& settings, int index, ecs::GameLoop& loop)
{
    if (session.settingsOpen) {
        activateSettings(session, settings, index);
        return;
    }
    if (layout::leaderboardOpen(session)) {
        if (index == layout::SoloTab) {
            showBoard(session, network::protocol::Board::Solo);
        } else if (index == layout::MultiTab) {
            showBoard(session, network::protocol::Board::Multi);
        } else {
            closeLeaderboard(session);
        }
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
    case Screen::Home:
        if (index == layout::PlayButton) {
            pressPlay(session);
        } else if (index == layout::HomeSettingsButton) {
            openSettings(session);
        } else if (index == layout::LogOutButton) {
            session.request = Request::Leave;
        } else if (index == layout::ChatBox) {
            openChat(session, std::nullopt);
        } else if (index == layout::LeaderboardButton) {
            openLeaderboard(session);
        } else if (index < layout::FriendNameField) {
            activateGroupButton(session, index);
        } else if (index == layout::FriendAddButton) {
            askFriend(session);
        } else if (index >= layout::FirstFriendButton) {
            const sf::FloatRect bar
                = layout::friendsBar(layout::visibleArea(session, settings.interfaceSize), true);
            activateFriendButton(session,
                layout::friendButtons(session, bar)
                    .at(static_cast<std::size_t>(index - layout::FirstFriendButton)));
        }
        break;
    case Screen::WaitingRoom:
        session.request = Request::LeaveGame;
        break;
    case Screen::GameOver:
        session.request = index == layout::PlayAgainButton && playsAlone(session)
            ? Request::Restart
            : Request::LeaveGame;
        break;
    case Screen::Game:
        if (index == layout::ResumeButton) {
            session.menuOpen = false;
        } else if (index == layout::MenuSettingsButton) {
            openSettings(session);
        } else {
            session.request = Request::LeaveGame;
        }
        break;
    }
}

void moveFocus(Session& session, const Settings& settings, int step)
{
    const auto parts = layout::focusables(session, settings.interfaceSize);
    const int count = static_cast<int>(parts.size());
    for (int tries = 0; tries < count; ++tries) {
        session.focus = (session.focus + step + count) % count;
        if (parts.at(static_cast<std::size_t>(session.focus)).size.x > 0.0F) {
            return; // A part that does not apply for now is skipped
        }
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

/// Esc: closes the settings, the menu or the friends panel, leaves the screen, or
/// quits. Nothing else on the main page: logging out takes its own button.
void escape(Session& session, ecs::GameLoop& loop)
{
    if (session.settingsOpen) {
        closeSettings(session);
    } else if (layout::leaderboardOpen(session)) {
        closeLeaderboard(session);
    } else if (session.screen == Screen::Connect) {
        session.request = Request::Quit;
        loop.stop();
    } else if (session.screen == Screen::Game) {
        session.menuOpen = !session.menuOpen;
        session.focus = 0;
    } else if (session.screen == Screen::Home) {
        session.focus = session.focus > layout::ChatBox ? layout::PlayButton : session.focus;
    } else {
        session.request = Request::LeaveGame;
    }
}

/// Enter, or A on a pad: activates the focus. In the form, logs in from any field;
/// in the friends panel, asks from the name field; in the code field, joins the group.
void confirm(Session& session, Settings& settings, ecs::GameLoop& loop)
{
    if (!session.settingsOpen && session.screen == Screen::Home
        && session.focus == layout::FriendNameField) {
        askFriend(session);
        return;
    }
    if (!session.settingsOpen && session.screen == Screen::Home
        && session.focus == layout::GroupCodeField) {
        joinWithCode(session);
        return;
    }
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
    if (session.chatOpen) {
        onChatKey(session, key.code);
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
    case Key::PageUp:
    case Key::PageDown:
        if (layout::friendsOpen(session)) {
            scrollFriends(session, settings.interfaceSize, key.code == Key::PageDown ? 1 : -1);
        } else if (session.screen == Screen::Home || session.screen == Screen::Game) {
            scrollChat(session, key.code == Key::PageUp ? 1 : -1);
        }
        break;
    case Key::Enter:
        if (!menu) {
            openChat(session, std::nullopt); // In the game, Enter talks
            break;
        }
        confirm(session, settings, loop);
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
    add(Action::Pod, input::Pod);
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
            if ((click->button != sf::Mouse::Button::Left
                    && click->button != sf::Mouse::Button::Right)
                || session.capturing) {
                continue;
            }
            const sf::FloatRect area = layout::visibleArea(session, settings.interfaceSize);
            const sf::Vector2f point
                = _window.mapPixelToCoords(click->position, letterboxed(area, _window.getSize()));
            if (click->button == sf::Mouse::Button::Right) {
                rightClick(session, area, point);
                continue;
            }
            if (leftClickOnChatOrMenu(session, settings, area, point)) {
                continue;
            }
            const auto targets = layout::focusables(session, settings.interfaceSize);
            bool hit = false;
            for (std::size_t i = 0; i < targets.size(); ++i) {
                if (targets.at(i).contains(point)) {
                    hit = true;
                    session.focus = static_cast<int>(i);
                    if (!clickSettings(session, settings, point)) {
                        activate(session, settings, session.focus, _loop);
                    }
                    break;
                }
            }
            // A click beside the panel closes it, unless the mouse is over it
            if (!hit && !session.friendsHovered && !session.settingsOpen
                && session.screen == Screen::Home && session.focus >= layout::FriendNameField) {
                session.focus = layout::PlayButton;
            }
        } else if (const auto* mouse = event->getIf<sf::Event::MouseMoved>()) {
            const sf::FloatRect area = layout::visibleArea(session, settings.interfaceSize);
            const sf::Vector2f point
                = _window.mapPixelToCoords(mouse->position, letterboxed(area, _window.getSize()));
            session.friendsHovered
                = layout::friendsBar(area, layout::friendsOpen(session)).contains(point);
        } else if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
            if (layout::friendsOpen(session) && wheel->delta != 0.0F) {
                scrollFriends(session, settings.interfaceSize, wheel->delta < 0.0F ? 1 : -1);
            } else if (wheel->delta != 0.0F
                && (session.screen == Screen::Home || session.screen == Screen::Game)) {
                scrollChat(session, wheel->delta > 0.0F ? 1 : -1);
            }
        }
    }
    if (!layout::friendsOpen(session)) {
        session.friendMenu.reset();
    }
    if (session.screen == Screen::Home && !session.settingsOpen) {
        // A row may have gone since the last frame: the focus and the scroll must still exist
        const int count
            = static_cast<int>(layout::focusables(session, settings.interfaceSize).size());
        session.focus = std::min(session.focus, count - 1);
        // The leader role or an invitation may have gone with the part that had the focus
        if (layout::focusables(session, settings.interfaceSize)
                .at(static_cast<std::size_t>(session.focus))
                .size.x
            <= 0.0F) {
            session.focus = layout::PlayButton;
        }
        const std::size_t shown = layout::friendRowsShown(
            layout::friendsBar(layout::visibleArea(session, settings.interfaceSize), true));
        const std::size_t last
            = session.friends.size() > shown ? session.friends.size() - shown : 0;
        session.friendScroll = std::min(session.friendScroll, last);
    }
    // Held keys and pads only steer the ship in the game itself, never through a menu
    std::uint8_t mask = 0;
    if (!inMenu(session) && !session.chatOpen && _window.hasFocus()) {
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
