#include "server/Systems/BydoFireSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Bydo.hpp"
#include "game/Components/Motion.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/Player.hpp"
#include "server/Spawn.hpp"

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace server {

void BydoFireSystem::update(ecs::World& world, const ecs::Time& time)
{
    std::vector<game::Position> ships;
    world.query<PlayerShip, game::Position>(
        [&](ecs::Entity /*entity*/, PlayerShip& /*ship*/, game::Position& position) {
            ships.push_back(position);
        });

    std::vector<std::pair<game::Position, game::Velocity>> shots; // Created after the query
    world.query<Bydo, game::Position>([&](ecs::Entity /*entity*/, Bydo& bydo,
                                          game::Position& position) {
        bydo.fireCooldown -= time.deltaSeconds();
        if (ships.empty() || bydo.fireCooldown > 0.0F) {
            return;
        }
        bydo.fireCooldown = game::bydo::FireInterval;

        float bestDistance = std::numeric_limits<float>::max();
        game::Velocity aim{.dx = -game::bydo::MissileSpeed, .dy = 0.0F};
        for (const game::Position& ship : ships) {
            const float dx = ship.x - position.x;
            const float dy = ship.y - position.y;
            const float distance = std::hypot(dx, dy);
            if (distance > 0.0F && distance < bestDistance) {
                bestDistance = distance;
                aim = {.dx = dx / distance * game::bydo::MissileSpeed,
                    .dy = dy / distance * game::bydo::MissileSpeed};
            }
        }
        shots.emplace_back(position, aim);
    });
    for (const auto& [muzzle, velocity] : shots) {
        (void)spawnEnemyMissile(world, muzzle, velocity);
    }
}

} // namespace server
