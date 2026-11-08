#include "server/Systems/AimSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Components/Motion.hpp"
#include "server/Components/Behaviour.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/Player.hpp"

#include <cmath>
#include <limits>
#include <vector>

namespace server {

void AimSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    std::vector<game::Position> ships;
    world.query<PlayerShip, game::Position>(
        [&](ecs::Entity /*entity*/, const PlayerShip& /*ship*/, const game::Position& position) {
            ships.push_back(position);
        });

    world.query<AimsAtShips, game::Position, Trigger>(
        [&](ecs::Entity /*entity*/, const AimsAtShips& /*aims*/, const game::Position& position,
            Trigger& trigger) {
            const bool onScreen = position.x < game::arena::Width;
            trigger.pulled = onScreen && !ships.empty();
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
