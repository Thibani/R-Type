#include "server/Systems/RoomSystem.hpp"

#include "ecs/World/World.hpp"
#include "network/Protocol/Packets.hpp"
#include "server/Components/ClientMessage.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/NetClient.hpp"
#include "server/GameRoom.hpp"
#include "server/Singleton.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <utility>
#include <variant>
#include <vector>

namespace server {

using namespace network::protocol;

void RoomMessageSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    std::vector<ecs::Entity> messages;
    world.query<ClientMessage>([&](ecs::Entity entity, ClientMessage& message) {
        if (std::holds_alternative<LeaveGame>(message.payload)
            || std::holds_alternative<Input>(message.payload)) {
            messages.push_back(entity);
        }
    });

    auto& rooms = singleton<GameRooms>(world).rooms;
    for (const ecs::Entity entity : messages) {
        ClientMessage message = std::move(world.getComponent<ClientMessage>(entity));
        world.destroyEntity(entity);
        const auto* place = world.tryGetComponent<InRoom>(message.client);
        if (place == nullptr || world.hasComponent<Disconnecting>(message.client)) {
            continue; // On the main page, or `SessionCleanupSystem` is announcing their departure
        }
        if (std::holds_alternative<LeaveGame>(message.payload)) {
            std::cout << "r-type_server: player " << static_cast<int>(place->playerId) << " '"
                      << world.getComponent<NetClient>(message.client).name
                      << "' went back to the main page\n";
            leaveRoom(world, message.client, LeaveReason::Quit);
        } else if (const auto room = rooms.find(place->room); room != rooms.end()) {
            ecs::World& game = room->second->world;
            game.addComponent(game.createEntity(),
                ClientMessage{.client = place->player, .payload = std::move(message.payload)});
        }
    }
}

RoomSystem::RoomSystem(ScoreStore& scores) noexcept
    : _scores(scores)
{
}

void RoomSystem::update(ecs::World& world, const ecs::Time& time)
{
    auto& rooms = singleton<GameRooms>(world).rooms;
    std::erase_if(rooms, [&](const auto& entry) {
        std::size_t players = 0;
        GameRoom& room = *entry.second;
        room.world.template query<InGame>(
            [&](ecs::Entity /*entity*/, InGame& /*player*/) { ++players; });
        if (players != 0) {
            return false;
        }
        // Nobody left: the game is over, and its score enters the leaderboard
        const std::uint32_t score = singleton<TeamScore>(room.world).value;
        if (_scores.add(score, room.names)) {
            std::cout << "r-type_server: game " << entry.first << " ended with " << score
                      << " points\n";
        }
        return true;
    });
    for (const auto& [id, room] : rooms) {
        room->loop.runFrame(time.deltaSeconds());
    }
}

} // namespace server
