#include "server/Systems/BydoAimSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/Player.hpp"

#include <cmath>
#include <limits>
#include <vector>

namespace server {

void BydoAimSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    std::vector<game::Position> ships;
    world.query<PlayerShip, game::Position>(
        [&](ecs::Entity /*entity*/, PlayerShip& /*ship*/, game::Position& position) {
            ships.push_back(position);
        });

    world.query<Bydo, game::Position, Trigger>([&](ecs::Entity /*entity*/, Bydo& /*bydo*/,
                                                   game::Position& position, Trigger& trigger) {
        trigger.pulled = !ships.empty();
        float bestDistance = std::numeric_limits<float>::max();
        for (const game::Position& ship : ships) {
            const float dx = ship.x - position.x;
            const float dy = ship.y - position.y;
            const float distance = std::hypot(dx, dy);
            if (distance > 0.0F && distance < bestDistance) {
                bestDistance = distance;
                trigger.direction = {.dx = dx / distance, .dy = dy / distance};
            }
        }
    });
}

} // namespace server
