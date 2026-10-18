#include "server/Systems/PlayerShipSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Components/Motion.hpp"
#include "network/Protocol/Protocol.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/NetEntity.hpp"
#include "server/Components/Player.hpp"
#include "server/NetIds.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace server {

using namespace network::protocol;

void PlayerShipSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    std::array<bool, MaxPlayers> connected{};
    std::array<bool, MaxPlayers> hasShip{};
    world.query<NetClient>(
        [&](ecs::Entity /*entity*/, NetClient& client) { connected.at(client.playerId) = true; });

    std::vector<ecs::Entity> orphans;
    world.query<PlayerShip>([&](ecs::Entity entity, PlayerShip& ship) {
        if (connected.at(ship.playerId)) {
            hasShip.at(ship.playerId) = true;
        } else {
            orphans.push_back(entity); // Its player left
        }
    });
    for (const ecs::Entity ship : orphans) {
        world.destroyEntity(ship);
    }

    for (std::uint8_t playerId = 0; playerId < MaxPlayers; ++playerId) {
        if (!connected.at(playerId) || hasShip.at(playerId)) {
            continue;
        }
        const auto spawn = game::arena::playerSpawn(playerId);
        const ecs::Entity ship = world.createEntity();
        world.addComponent(ship, PlayerShip{.playerId = playerId});
        world.addComponent(ship, game::Position{.x = spawn.x, .y = spawn.y});
        world.addComponent(ship, game::Velocity{});
        world.addComponent(ship,
            NetEntity{
                .netId = allocateNetId(world),
                .kind = EntityKind::PlayerShip,
                .owner = playerId,
            });
    }
}

} // namespace server
