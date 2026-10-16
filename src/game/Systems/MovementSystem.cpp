#include "game/Systems/MovementSystem.hpp"

#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"

namespace game {

void MovementSystem::update(ecs::World& world, const ecs::Time& time)
{
    const float delta = time.deltaSeconds();
    world.query<Position, Velocity>(
        [&](ecs::Entity /*entity*/, Position& position, Velocity& velocity) {
            position.x += velocity.dx * delta;
            position.y += velocity.dy * delta;
        });
}

} // namespace game
