#include "client/Systems/NetReceiveSystem.hpp"

#include "client/Components/Session.hpp"
#include "client/Network/ServerConnection.hpp"
#include "client/Replication.hpp"
#include "client/Singleton.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"

#include <string>

namespace client {

NetReceiveSystem::NetReceiveSystem(ServerConnection& connection) noexcept
    : _connection(connection)
{
}

void NetReceiveSystem::update(ecs::World& world, const ecs::Time& time)
{
    const auto now = ServerConnection::Clock::now();
    const bool wasConnected = _connection.status() == ServerConnection::Status::Connected;
    const auto payloads = _connection.receive(now);

    Session& session = singleton<Session>(world);
    if (!wasConnected && _connection.accepted()) {
        startSession(world, *_connection.accepted(), session.name);
    }
    for (const auto& payload : payloads) {
        applyPayload(world, payload, time.elapsedSeconds());
    }

    switch (_connection.status()) {
    case ServerConnection::Status::Connecting:
        session.status = "Connecting... attempt " + std::to_string(_connection.attempts()) + " / "
            + std::to_string(ServerConnection::MaxAttempts);
        break;
    case ServerConnection::Status::Failed:
        // Here, or detected by update() last frame: back to the form with the reason
        clearSession(world);
        session.screen = Screen::Connect;
        session.focus = 0;
        session.status.clear();
        session.error = describe(_connection.failure());
        _connection.disconnect(now); // Failed → Idle: reported once
        break;
    case ServerConnection::Status::Idle:
    case ServerConnection::Status::Connected:
        break;
    }
}

} // namespace client
