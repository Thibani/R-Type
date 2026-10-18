#include "server/Systems/PlayerFireSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "game/Ship.hpp"
#include "network/Protocol/Protocol.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/Player.hpp"
#include "server/Spawn.hpp"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace server {

using namespace network::protocol;

void PlayerFireSystem::update(ecs::World& world, const ecs::Time& time)
{
    std::array<bool, MaxPlayers> firing{};
    world.query<NetClient, PlayerInput>(
        [&](ecs::Entity /*entity*/, NetClient& client, PlayerInput& input) {
            firing.at(client.playerId) = input.received && (input.mask & input::Fire) != 0;
        });

    std::vector<std::pair<std::uint8_t, game::Position>> shots; // Created after the query
    world.query<PlayerShip, game::Position, FireCooldown>([&](ecs::Entity /*entity*/,
                                                              PlayerShip& ship,
                                                              game::Position& position,
                                                              FireCooldown& cooldown) {
        cooldown.remaining -= time.deltaSeconds();
        if (!firing.at(ship.playerId) || cooldown.remaining > 0.0F) {
            return;
        }
        cooldown.remaining = game::ship::FireCooldown;
        shots.emplace_back(ship.playerId,
            game::Position{.x = position.x + game::ship::HalfWidth, .y = position.y});
    });
    for (const auto& [playerId, muzzle] : shots) {
        (void)spawnPlayerMissile(world, playerId, muzzle);
    }
}

} // namespace server
