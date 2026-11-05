#include "server/Systems/HopSystem.hpp"

#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "server/Components/Behaviour.hpp"

namespace server {

void HopSystem::update(ecs::World& world, const ecs::Time& time)
{
    world.query<Hop, game::Position, game::Velocity>([&](ecs::Entity /*entity*/, const Hop& hop,
                                                         const game::Position& position,
                                                         game::Velocity& velocity) {
        const bool landed = position.y >= hop.floorY && velocity.dy >= 0.0F;
        velocity.dy = landed ? -hop.jumpSpeed : velocity.dy + (hop.gravity * time.deltaSeconds());
    });
}

} // namespace server
