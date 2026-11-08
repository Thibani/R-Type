#include "server/GameRoom.hpp"

#include "network/Protocol/Packets.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/NetEntity.hpp"
#include "server/ServerSystems.hpp"
#include "server/Singleton.hpp"
#include "server/Systems/ConnectionSystem.hpp"
#include "server/Systems/InputSystem.hpp"
#include "server/Systems/PlayerShipSystem.hpp"
#include "server/Systems/RestartSystem.hpp"
#include "server/Systems/ReplicationSystem.hpp"

#include <cstddef>
#include <iostream>
#include <utility>

namespace server {

using namespace network::protocol;

GameRoom::GameRoom(ecs::World& sessions, std::uint32_t firstTick)
{
    singleton<SimulationTick>(world).value = firstTick;
    loop.frameStart().add<InputSystem>();
    loop.frameStart().add<RestartSystem>();
    loop.frameStart().add<PlayerShipSystem>();
    installSimulation(world);
    loop.frameEnd().add<ReplicationSystem>(world, sessions);
}

void startGame(ecs::World& sessions, std::span<const ecs::Entity> players, float now)
{
    auto& rooms = singleton<GameRooms>(sessions);
    const std::uint32_t id = rooms.nextId++;
    auto room = std::make_unique<GameRoom>(
        sessions, static_cast<std::uint32_t>(now * ConnectionSystem::TickRate));

    for (std::size_t place = 0; place < players.size(); ++place) {
        const ecs::Entity session = players[place];
        const auto playerId = static_cast<std::uint8_t>(place);
        const ecs::Entity player = room->world.createEntity();
        room->world.addComponent(player, InGame{.playerId = playerId, .session = session});
        sessions.addComponent(session, InRoom{.room = id, .player = player, .playerId = playerId});

        NetClient& client = sessions.getComponent<NetClient>(session);
        room->names.push_back(client.name);
        client.channel.queueReliable(JoinResult{.code = JoinCode::Ok, .playerId = playerId});
        for (std::size_t other = 0; other < players.size(); ++other) {
            if (other != place) {
                client.channel.queueReliable(PlayerJoined{
                    .playerId = static_cast<std::uint8_t>(other),
                    .name = sessions.getComponent<NetClient>(players[other]).name,
                });
            }
        }
        std::cout << "r-type_server: player " << static_cast<int>(playerId) << " '" << client.name
                  << "' joined game " << id << " from " << client.endpoint << '\n';
    }
    rooms.rooms.emplace(id, std::move(room));
}

void leaveRoom(ecs::World& sessions, ecs::Entity session, LeaveReason reason)
{
    const auto* where = sessions.tryGetComponent<InRoom>(session);
    if (where == nullptr) {
        return;
    }
    const InRoom place = *where;
    sessions.removeComponent<InRoom>(session);
    NetClient& client = sessions.getComponent<NetClient>(session);
    client.knownEntities.clear();
    client.knownState.reset();

    auto& rooms = singleton<GameRooms>(sessions).rooms;
    const auto found = rooms.find(place.room);
    if (found == rooms.end()) {
        return;
    }
    ecs::World& world = found->second->world;
    world.destroyEntity(place.player);
    world.query<InGame>([&](ecs::Entity /*entity*/, InGame& other) {
        sessions.getComponent<NetClient>(other.session)
            .channel.queueReliable(PlayerLeft{.playerId = place.playerId, .reason = reason});
    });
}

} // namespace server
