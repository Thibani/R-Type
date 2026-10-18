#include "server/Systems/BydoMovementSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Bydo.hpp"
#include "game/Components/Motion.hpp"
#include "server/Components/Combat.hpp"

#include <cmath>

namespace server {

void BydoMovementSystem::update(ecs::World& world, const ecs::Time& time)
{
    using namespace game::bydo;
    world.query<Bydo, game::Velocity>(
        [&](ecs::Entity /*entity*/, Bydo& bydo, game::Velocity& velocity) {
            bydo.age += time.deltaSeconds();
            // d/dt (A sin(ωt)) = Aω cos(ωt)
            velocity = {.dx = -Speed,
                .dy = WaveAmplitude * WaveFrequency * std::cos(WaveFrequency * bydo.age)};
        });
}

} // namespace server
