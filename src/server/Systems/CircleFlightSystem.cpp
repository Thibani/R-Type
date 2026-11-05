#include "server/Systems/CircleFlightSystem.hpp"

#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "server/Components/Behaviour.hpp"

#include <cmath>

namespace server {

void CircleFlightSystem::update(ecs::World& world, const ecs::Time& time)
{
    world.query<CircleFlight, game::Velocity>(
        [&](ecs::Entity /*entity*/, CircleFlight& loop, game::Velocity& velocity) {
            loop.angle += loop.loopSpeed * time.deltaSeconds();
            const float tangential = loop.radius * loop.loopSpeed;
            velocity = {
                .dx = -loop.speed - (tangential * std::sin(loop.angle)),
                .dy = tangential * std::cos(loop.angle),
            };
        });
}

} // namespace server
