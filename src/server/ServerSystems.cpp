#include "server/ServerSystems.hpp"

#include "game/Systems/MovementSystem.hpp"
#include "server/Systems/ConnectionSystem.hpp"
#include "server/Systems/DiscardPacketsSystem.hpp"
#include "server/Systems/InputSystem.hpp"
#include "server/Systems/NetReceiveSystem.hpp"
#include "server/Systems/NetSendSystem.hpp"
#include "server/Systems/PlayerBoundsSystem.hpp"
#include "server/Systems/PlayerControlSystem.hpp"
#include "server/Systems/PlayerShipSystem.hpp"
#include "server/Systems/ReplicationSystem.hpp"
#include "server/Systems/SessionCleanupSystem.hpp"
#include "server/Systems/SessionReceiveSystem.hpp"
#include "server/Systems/SnapshotSystem.hpp"
#include "server/Systems/TickSystem.hpp"
#include "server/Systems/TimeoutSystem.hpp"

namespace server {

void installSystems(ecs::World& world, ecs::GameLoop& loop, network::NetworkThread& socket)
{
    loop.frameStart().add<NetReceiveSystem>(socket);
    loop.frameStart().add<ConnectionSystem>(socket);
    loop.frameStart().add<SessionReceiveSystem>();
    loop.frameStart().add<TimeoutSystem>();
    loop.frameStart().add<InputSystem>();
    loop.frameStart().add<PlayerShipSystem>();

    world.systems().add<TickSystem>();
    world.systems().add<PlayerControlSystem>();
    world.systems().add<game::MovementSystem>();
    world.systems().add<PlayerBoundsSystem>();

    loop.frameEnd().add<SessionCleanupSystem>(socket);
    loop.frameEnd().add<ReplicationSystem>();
    loop.frameEnd().add<NetSendSystem>(socket);
    loop.frameEnd().add<SnapshotSystem>(socket);
    loop.frameEnd().add<DiscardPacketsSystem>();
}

} // namespace server
