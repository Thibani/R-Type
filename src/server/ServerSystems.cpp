#include "server/ServerSystems.hpp"

#include "game/Systems/ArenaExitSystem.hpp"
#include "game/Systems/CollisionSystem.hpp"
#include "game/Systems/DespawnSystem.hpp"
#include "game/Systems/MovementSystem.hpp"
#include "server/Components/Combat.hpp"
#include "server/Singleton.hpp"
#include "server/Systems/BydoFireSystem.hpp"
#include "server/Systems/BydoMovementSystem.hpp"
#include "server/Systems/BydoSpawnSystem.hpp"
#include "server/Systems/DamageSystem.hpp"
#include "server/Systems/PlayerFireSystem.hpp"
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

void installSimulation(ecs::World& world)
{
    (void)singleton<TeamScore>(world); // Created now: DamageSystem adds to it during events

    world.systems().add<TickSystem>();
    world.systems().add<PlayerControlSystem>();
    world.systems().add<PlayerFireSystem>();
    world.systems().add<BydoSpawnSystem>();
    world.systems().add<BydoMovementSystem>();
    world.systems().add<BydoFireSystem>();
    world.systems().add<game::MovementSystem>();
    world.systems().add<PlayerBoundsSystem>();
    world.systems().add<game::ArenaExitSystem>();
    world.systems().add<game::CollisionSystem>();
    world.systems().add<DamageSystem>(world);
    world.systems().add<game::DespawnSystem>(world);
}

void installSystems(ecs::World& world, ecs::GameLoop& loop, network::NetworkThread& socket)
{
    loop.frameStart().add<NetReceiveSystem>(socket);
    loop.frameStart().add<ConnectionSystem>(socket);
    loop.frameStart().add<SessionReceiveSystem>();
    loop.frameStart().add<TimeoutSystem>();
    loop.frameStart().add<InputSystem>();
    loop.frameStart().add<PlayerShipSystem>();

    installSimulation(world);

    loop.frameEnd().add<SessionCleanupSystem>(socket);
    loop.frameEnd().add<ReplicationSystem>(world);
    loop.frameEnd().add<NetSendSystem>(socket);
    loop.frameEnd().add<SnapshotSystem>(socket);
    loop.frameEnd().add<DiscardPacketsSystem>();
}

} // namespace server
