#include "game/Systems/MovementSystem.hpp"

#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"

namespace game {

void MovementSystem::update(ecs::World& world, const ecs::Time& time)
{
    const float deltaSeconds = time.deltaSeconds();

    world.query<Position, Velocity>(
        [deltaSeconds](ecs::Entity /*entity*/, Position& position, const Velocity& velocity) {
            position.x += velocity.dx * deltaSeconds;
            position.y += velocity.dy * deltaSeconds;
        });
}

} // namespace game
