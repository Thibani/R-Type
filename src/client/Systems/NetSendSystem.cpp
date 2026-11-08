#include "client/Systems/NetSendSystem.hpp"

#include "client/ClientOptions.hpp"
#include "client/Components/Controls.hpp"
#include "client/Components/Session.hpp"
#include "client/Network/ServerConnection.hpp"
#include "client/Replication.hpp"
#include "client/Singleton.hpp"
#include "ecs/World/World.hpp"
#include "network/Protocol/PacketCodec.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace client {

using namespace network::protocol;

namespace {

/// Checks the form, then starts connecting. Errors are shown under the form.
void connect(Session& session, ServerConnection& connection, AuthMode mode,
    ServerConnection::Clock::time_point now)
{
    if (connection.status() == ServerConnection::Status::LoggingIn) {
        return; // Answer on its way: a second connection would be refused
    }
    if (session.name.empty() || !isValidName(session.name)) {
        session.error = "Enter a name of 1 to 16 characters.";
        return;
    }
    if (session.password.empty()
        || (mode == AuthMode::Register && session.password.size() < MinPasswordSize)) {
        session.error = "Enter a password of 8 to 64 characters.";
        return;
    }
    const auto port = parsePort(session.port);
    if (!port) {
        session.error = "Enter a port between 1 and 65535.";
        return;
    }
    session.error.clear();
    connection.connect(session.address, *port,
        {.name = session.name, .password = session.password, .mode = mode}, now);
}

} // namespace

NetSendSystem::NetSendSystem(ServerConnection& connection) noexcept
    : _connection(connection)
{
}

void NetSendSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    const auto now = ServerConnection::Clock::now();
    Session& session = singleton<Session>(world);

    if (session.request == Request::Login) {
        connect(session, _connection, AuthMode::Login, now);
    } else if (session.request == Request::CreateAccount) {
        connect(session, _connection, AuthMode::Register, now);
    } else if (session.request == Request::Restart) {
        _connection.sendRestart(now); // GAME_STATE (RUNNING) brings the game back
    } else if (session.request == Request::Leave) {
        _connection.disconnect(now);
        clearSession(world);
        session.screen = Screen::Connect;
        session.focus = 0;
        session.status.clear();
        session.error.clear();
    } else if (session.request == Request::Play) {
        session.error.clear();
        _connection.sendReliable(JoinGame{}); // A second click during the countdown does nothing
    } else if (session.request == Request::LeaveGame) {
        _connection.sendReliable(LeaveGame{});
        leaveGame(world);
    }
    for (network::protocol::Payload& payload : session.outgoing) {
        _connection.sendReliable(std::move(payload));
    }
    session.outgoing.clear();
    if (session.request != Request::Quit) {
        session.request = Request::None; // Quit is for main(), after the loop
    }

    InputHistory& history = singleton<InputHistory>(world);
    if (history.unsent && _connection.status() == ServerConnection::Status::Connected) {
        Input input{.firstTick = history.firstTick,
            .count = static_cast<std::uint8_t>(history.masks.size()),
            .masks = {}};
        std::ranges::copy(history.masks, input.masks.begin());
        _connection.sendInput(input, now);
        history.unsent = false;
    }

    _connection.update(now);
}

} // namespace client
