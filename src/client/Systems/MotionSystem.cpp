#include "client/Systems/MotionSystem.hpp"

#include "client/Components/NetEntity.hpp"
#include "client/Components/Session.hpp"
#include "client/Singleton.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"

namespace client {

void MotionSystem::update(ecs::World& world, const ecs::Time& time)
{
    const float step = time.deltaSeconds();
    // A paused game stops on the server: what came from it stops here too. The sky still moves
    const bool paused = gamePaused(singleton<Session>(world));
    world.query<game::Position, game::Velocity>(
        [&](ecs::Entity entity, game::Position& position, game::Velocity& velocity) {
            if (paused && world.hasComponent<NetEntity>(entity)) {
                return;
            }
            position.x += velocity.dx * step;
            position.y += velocity.dy * step;
        });
}

} // namespace client
