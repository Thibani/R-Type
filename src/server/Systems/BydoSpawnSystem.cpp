#include "server/Systems/BydoSpawnSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Bydo.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/Player.hpp"
#include "server/Singleton.hpp"
#include "server/Spawn.hpp"

#include <random>

namespace server {

void BydoSpawnSystem::update(ecs::World& world, const ecs::Time& time)
{
    bool anyShip = false;
    world.query<PlayerShip>([&](ecs::Entity /*entity*/, PlayerShip& /*ship*/) { anyShip = true; });
    if (!anyShip) {
        return; // Nobody to fight: the arena stays empty
    }

    auto& spawner = singleton<BydoSpawner>(world);
    spawner.untilNext -= time.deltaSeconds();
    if (spawner.untilNext > 0.0F) {
        return;
    }
    spawner.untilNext += game::bydo::SpawnInterval;

    // The whole wave stays inside the arena
    constexpr float margin = game::bydo::HalfHeight + game::bydo::WaveAmplitude;
    std::uniform_real_distribution<float> height(margin, game::arena::Height - margin);
    std::uniform_real_distribution<float> firstShot(0.5F, game::bydo::FireInterval);
    const float baseY = height(spawner.random);
    const float delay = firstShot(spawner.random);
    (void)spawnBydo(world, baseY, delay);
}

} // namespace server
