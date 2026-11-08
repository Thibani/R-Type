#include "server/Systems/PlayerShipSystem.hpp"

#include "ecs/World/World.hpp"
#include "network/Protocol/Protocol.hpp"
#include "server/Components/Match.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/Player.hpp"
#include "server/Singleton.hpp"
#include "server/Spawn.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace server {

using namespace network::protocol;

void PlayerShipSystem::update(ecs::World& world, const ecs::Time& time)
{
    std::array<bool, MaxPlayers> connected{};
    std::array<bool, MaxPlayers> hasShip{};
    world.query<InGame>(
        [&](ecs::Entity /*entity*/, InGame& player) { connected.at(player.playerId) = true; });

    // Out of lives, or the game is over: no new ship until the next game
    const bool gameOver = singleton<Match>(world).phase != GamePhase::Running;
    world.query<InGame, Eliminated>(
        [&](ecs::Entity /*entity*/, InGame& player, Eliminated& /*out*/) {
            hasShip.at(player.playerId) = true;
        });

    // A destroyed ship comes back after its delay: until then, the player waits
    std::vector<ecs::Entity> respawned;
    world.query<InGame, RespawnTimer>([&](ecs::Entity entity, InGame& player, RespawnTimer& timer) {
        timer.remaining -= time.deltaSeconds();
        if (timer.remaining > 0.0F) {
            hasShip.at(player.playerId) = true; // Not yet
        } else {
            respawned.push_back(entity);
        }
    });
    for (const ecs::Entity player : respawned) {
        world.removeComponent<RespawnTimer>(player);
    }

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
        if (gameOver || !connected.at(playerId) || hasShip.at(playerId)) {
            continue;
        }
        (void)spawnPlayerShip(world, playerId);
    }
}

} // namespace server
