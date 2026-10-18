#include "client/Systems/MotionSystem.hpp"

#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"

namespace client {

void MotionSystem::update(ecs::World& world, const ecs::Time& time)
{
    const float step = time.deltaSeconds();
    world.query<game::Position, game::Velocity>(
        [step](ecs::Entity /*entity*/, game::Position& position, game::Velocity& velocity) {
            position.x += velocity.dx * step;
            position.y += velocity.dy * step;
        });
}

} // namespace client
