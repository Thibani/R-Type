#include "server/ServerSystems.hpp"

#include "game/Systems/ArenaExitSystem.hpp"
#include "game/Systems/CollisionSystem.hpp"
#include "game/Systems/ConfinementSystem.hpp"
#include "game/Systems/DespawnSystem.hpp"
#include "game/Systems/MovementSystem.hpp"
#include "server/Components/Combat.hpp"
#include "server/GameRoom.hpp"
#include "server/Groups.hpp"
#include "server/Singleton.hpp"
#include "server/Systems/BydoAimSystem.hpp"
#include "server/Systems/BydoMovementSystem.hpp"
#include "server/Systems/BydoSpawnSystem.hpp"
#include "server/Systems/ChatSystem.hpp"
#include "server/Systems/ConnectionSystem.hpp"
#include "server/Systems/DamageSystem.hpp"
#include "server/Systems/DiscardPacketsSystem.hpp"
#include "server/Systems/FriendSystem.hpp"
#include "server/Systems/GroupSystem.hpp"
#include "server/Systems/LeaderboardSystem.hpp"
#include "server/Systems/LoginSystem.hpp"
#include "server/Systems/NetReceiveSystem.hpp"
#include "server/Systems/NetSendSystem.hpp"
#include "server/Systems/PlayerControlSystem.hpp"
#include "server/Systems/ReplicationSystem.hpp"
#include "server/Systems/RoomSystem.hpp"
#include "server/Systems/SessionCleanupSystem.hpp"
#include "server/Systems/SessionReceiveSystem.hpp"
#include "server/Systems/SnapshotSystem.hpp"
#include "server/Systems/TickSystem.hpp"
#include "server/Systems/TimeoutSystem.hpp"
#include "server/Systems/WeaponSystem.hpp"

namespace server {

void installSimulation(ecs::World& world)
{
    (void)singleton<TeamScore>(world); // Created now: DamageSystem adds to it during events

    world.systems().add<TickSystem>();
    world.systems().add<PlayerControlSystem>();
    world.systems().add<BydoSpawnSystem>();
    world.systems().add<BydoMovementSystem>();
    world.systems().add<BydoAimSystem>();
    world.systems().add<WeaponSystem>();
    world.systems().add<game::MovementSystem>();
    world.systems().add<game::ConfinementSystem>();
    world.systems().add<game::ArenaExitSystem>();
    world.systems().add<game::CollisionSystem>();
    world.systems().add<DamageSystem>(world);
    world.systems().add<game::DespawnSystem>(world);
}

void installSystems(ecs::World& world, ecs::GameLoop& loop, network::NetworkThread& socket,
    AccountStore& accounts, FriendStore& friends, MessageStore& messages, ScoreStore& scores)
{
    (void)singleton<Groups>(world); // Created now, like TeamScore: read during the frame
    (void)singleton<GameRooms>(world);

    loop.frameStart().add<NetReceiveSystem>(socket);
    loop.frameStart().add<ConnectionSystem>(socket);
    loop.frameStart().add<SessionReceiveSystem>();
    loop.frameStart().add<LoginSystem>(accounts);
    loop.frameStart().add<RoomMessageSystem>();
    loop.frameStart().add<TimeoutSystem>();
    loop.frameStart().add<FriendSystem>(accounts, friends);
    loop.frameStart().add<GroupSystem>(friends);
    loop.frameStart().add<ChatSystem>(friends, messages);
    loop.frameStart().add<LeaderboardSystem>(scores);

    loop.frameEnd().add<SessionCleanupSystem>(socket);
    loop.frameEnd().add<RoomSystem>(scores);
    loop.frameEnd().add<NetSendSystem>(socket);
    loop.frameEnd().add<SnapshotSystem>(socket);
    loop.frameEnd().add<DiscardPacketsSystem>();
}

} // namespace server
