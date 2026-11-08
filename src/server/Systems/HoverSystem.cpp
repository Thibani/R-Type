#include "server/Systems/HoverSystem.hpp"

#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "server/Components/Behaviour.hpp"

#include <cmath>

namespace server {

void HoverSystem::update(ecs::World& world, const ecs::Time& time)
{
    world.query<Hover, game::Position, game::Velocity>([&](ecs::Entity /*entity*/, Hover& hover,
                                                           const game::Position& position,
                                                           game::Velocity& velocity) {
        const bool leaving = hover.holdSeconds > 0.0F && hover.held >= hover.holdSeconds;
        if (leaving || position.x > hover.stopX) {
            velocity = {.dx = -hover.speed, .dy = 0.0F};
            return;
        }
        hover.held += time.deltaSeconds();
        // d/dt (A sin(ωt)) = Aω cos(ωt): it bobs around the height where it stopped
        velocity = {
            .dx = 0.0F,
            .dy
            = hover.bobAmplitude * hover.bobFrequency * std::cos(hover.bobFrequency * hover.held),
        };
    });
}

} // namespace server
