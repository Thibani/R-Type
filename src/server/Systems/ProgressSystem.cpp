#include "server/Systems/ProgressSystem.hpp"

#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Run.hpp"
#include "game/Waves.hpp"
#include "server/Components/Combat.hpp"
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

    // Distance points: earned continuously, added to the score as whole points
    progress.distancePoints += time.deltaSeconds()
        * static_cast<float>(game::run::DistancePointsPerSecond * progress.level);
    const auto whole = static_cast<std::uint32_t>(progress.distancePoints);
    progress.distancePoints -= static_cast<float>(whole);
    singleton<TeamScore>(world).value += whole;
}

} // namespace server
