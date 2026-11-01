#include "server/Systems/EnemySpawnSystem.hpp"

#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Bydo.hpp"
#include "game/Enemies.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/Player.hpp"
#include "server/Singleton.hpp"
#include "server/Spawn.hpp"

#include <array>
#include <random>

namespace server {

namespace {

/// One line of the spawn table: how to create the enemy, how often it is
/// drawn, and how far from the top and bottom edges its centre line must be.
struct SpawnEntry {
    ecs::Entity (*spawn)(ecs::World& world, float y, float firstShotDelay);
    int weight;
    float margin;
};

constexpr std::array SpawnTable{
    SpawnEntry{.spawn = spawnBydo,
        .weight = 40,
        .margin = game::bydo::HalfHeight + game::bydo::WaveAmplitude},
    SpawnEntry{.spawn = spawnPataPata,
        .weight = 25,
        .margin = game::patapata::HalfHeight + game::patapata::WaveAmplitude},
    SpawnEntry{.spawn = spawnBug, .weight = 20, .margin = game::bug::HalfHeight},
    SpawnEntry{.spawn = spawnCancer,
        .weight = 10,
        .margin = game::cancer::HalfHeight + game::cancer::WaveAmplitude},
    SpawnEntry{.spawn = spawnCruiser, .weight = 5, .margin = game::cruiser::HalfHeight},
};

} // namespace

void EnemySpawnSystem::update(ecs::World& world, const ecs::Time& time)
{
    bool anyShip = false;
    world.query<PlayerShip>([&](ecs::Entity /*entity*/, PlayerShip& /*ship*/) { anyShip = true; });
    if (!anyShip) {
        return; // Nobody to fight: the arena stays empty
    }

    auto& spawner = singleton<EnemySpawner>(world);
    spawner.untilNext -= time.deltaSeconds();
    if (spawner.untilNext > 0.0F) {
        return;
    }
    spawner.untilNext += game::enemies::SpawnInterval;

    std::array<int, SpawnTable.size()> weights{};
    for (std::size_t i = 0; i < SpawnTable.size(); ++i) {
        weights.at(i) = SpawnTable.at(i).weight;
    }
    std::discrete_distribution<std::size_t> pick(weights.begin(), weights.end());
    const SpawnEntry& entry = SpawnTable.at(pick(spawner.random));

    std::uniform_real_distribution<float> height(entry.margin, game::arena::Height - entry.margin);
    std::uniform_real_distribution<float> firstShot(0.5F, 2.0F);
    const float y = height(spawner.random);
    const float delay = firstShot(spawner.random);
    (void)entry.spawn(world, y, delay);
}

} // namespace server
