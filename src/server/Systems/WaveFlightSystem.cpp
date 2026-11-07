#include "server/Systems/WaveFlightSystem.hpp"

#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "server/Components/Behaviour.hpp"

#include <cmath>

namespace server {

void WaveFlightSystem::update(ecs::World& world, const ecs::Time& time)
{
    world.query<WaveFlight, game::Velocity>(
        [&](ecs::Entity /*entity*/, WaveFlight& wave, game::Velocity& velocity) {
            wave.age += time.deltaSeconds();
            // d/dt (A sin(ωt)) = Aω cos(ωt)
            velocity = {.dx = -wave.speed,
                .dy = wave.amplitude * wave.frequency * std::cos(wave.frequency * wave.age)};
        });
}

} // namespace server
