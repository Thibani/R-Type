#include "server/Systems/SessionCleanupSystem.hpp"

#include "ecs/World/World.hpp"
#include "server/Components/NetClient.hpp"
#include "server/GameRoom.hpp"
#include "server/Network.hpp"

#include <cstddef>
#include <iostream>
#include <string_view>
#include <vector>

namespace server {

using namespace network::protocol;

namespace {

std::string_view describe(LeaveReason reason) noexcept
{
    switch (reason) {
    case LeaveReason::Quit:
        return "quit";
    case LeaveReason::Timeout:
        return "timed out";
    case LeaveReason::Kicked:
        return "was kicked";
    }
    return "left";
}

} // namespace

SessionCleanupSystem::SessionCleanupSystem(network::NetworkThread& socket) noexcept
    : _socket(socket)
{
}

void SessionCleanupSystem::update(ecs::World& world, const ecs::Time& time)
{
    const TimePoint now = toTimePoint(time);

    std::vector<ecs::Entity> leaving;
    std::vector<LeaveReason> reasons; // Of those in a game, in order
    world.query<NetClient, Disconnecting>(
        [&](ecs::Entity entity, NetClient& client, Disconnecting& disconnecting) {
            if (disconnecting.notify) {
                for (int i = 0; i < 3; ++i) { // No acknowledgement: repeat instead (§8.3)
                    const Packet packet
                        = client.channel.wrap(Disconnect{.reason = *disconnecting.notify}, now);
                    sendToClient(_socket, client, packet);
                }
            }
            leaving.push_back(entity);
            const auto* player = world.tryGetComponent<InRoom>(entity);
            if (player == nullptr) {
                return; // Not in a game: the players do not know about it
            }
            std::cout << "r-type_server: player " << static_cast<int>(player->playerId) << " '"
                      << client.name << "' " << describe(disconnecting.reason) << '\n';
            reasons.push_back(disconnecting.reason);
        });

    std::size_t inGame = 0;
    for (const ecs::Entity entity : leaving) {
        if (world.hasComponent<InRoom>(entity)) {
            leaveRoom(world, entity, reasons.at(inGame++));
        }
        world.destroyEntity(entity);
    }
}

} // namespace server
