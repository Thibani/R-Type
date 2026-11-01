#include "server/Systems/ProgressSystem.hpp"

#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Waves.hpp"
#include "server/Components/Player.hpp"
#include "server/Components/Waves.hpp"
#include "server/Singleton.hpp"

namespace server {

void ProgressSystem::update(ecs::World& world, const ecs::Time& time)
{
    bool anyShip = false;
    world.query<PlayerShip>(
        [&](ecs::Entity /*entity*/, const PlayerShip& /*ship*/) { anyShip = true; });
    if (!anyShip) {
        return;
    }
    auto& progress = singleton<RunProgress>(world);
    progress.elapsed += time.deltaSeconds();
    progress.level = game::waves::levelAt(progress.elapsed);
}

} // namespace server
