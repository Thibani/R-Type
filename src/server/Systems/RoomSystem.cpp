#include "server/Systems/RoomSystem.hpp"

#include "ecs/World/World.hpp"
#include "network/Protocol/Packets.hpp"
#include "server/Components/ClientMessage.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/Match.hpp"
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
            || std::holds_alternative<Input>(message.payload)
            || std::holds_alternative<Restart>(message.payload)
            || std::holds_alternative<Pause>(message.payload)) {
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
        const auto room = rooms.find(place->room);
        if (room == rooms.end()) {
            continue;
        }
        GameRoom& game = *room->second;
        const bool together = game.names.size() > 1; // Started by a group
        const bool over = singleton<Match>(game.world).phase == GamePhase::GameOver;
        if (std::holds_alternative<LeaveGame>(message.payload)) {
            // After the game over of a group, nobody stays alone in it: the whole group goes
            // back to the main page, and starts again from there with Ready
            std::vector<ecs::Entity> leaving{message.client};
            if (together && over) {
                game.world.query<InGame>([&](ecs::Entity /*entity*/, InGame& player) {
                    if (player.session != message.client) {
                        leaving.push_back(player.session);
                    }
                });
            }
            for (const ecs::Entity session : leaving) {
                std::cout << "r-type_server: '" << world.getComponent<NetClient>(session).name
                          << "' went back to the main page\n";
                leaveRoom(world, session, LeaveReason::Quit);
            }
        } else if (std::holds_alternative<Restart>(message.payload) && together) {
            continue; // Play again is for a game alone; a group plays again from the main page
        } else if (const auto* pause = std::get_if<Pause>(&message.payload)) {
            // Only a game alone stops: in a group's game, the others keep playing
            game.paused = !together && !over && pause->paused;
        } else if (game.paused) {
            continue; // Nothing moves: inputs would only pile up until the game resumes
        } else {
            game.world.addComponent(game.world.createEntity(),
                ClientMessage{.client = place->player, .payload = std::move(message.payload)});
        }
    }
}

RoomSystem::RoomSystem(ScoreStore& scores) noexcept
    : _scores(scores)
{
}

void RoomSystem::score(std::uint32_t id, GameRoom& room)
{
    if (room.scored) {
        return;
    }
    room.scored = true;
    const std::uint32_t points = singleton<TeamScore>(room.world).value;
    if (_scores.add(points, room.names)) {
        std::cout << "r-type_server: game " << id << " ended with " << points << " points\n";
    }
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
        score(entry.first, room); // Everyone left before the game over: it ends here
        return true;
    });
    for (const auto& [id, room] : rooms) {
        if (room->paused) {
            continue; // Its player is in the Esc menu: no step, no enemy, no damage
        }
        room->loop.runFrame(time.deltaSeconds());
        // No life and no ship left: the game is over, and its score enters the leaderboard.
        // "Play again" (RESTART) starts a new one in the same room
        if (singleton<Match>(room->world).phase == GamePhase::GameOver) {
            score(id, *room);
        } else if (room->scored) {
            room->scored = false;
        }
    }
}

} // namespace server
