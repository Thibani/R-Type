#include "server/Systems/WaveSystem.hpp"

#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Bydo.hpp"
#include "game/Enemies.hpp"
#include "game/Waves.hpp"
#include "server/Components/Player.hpp"
#include "server/Components/Waves.hpp"
#include "server/Singleton.hpp"
#include "server/Spawn.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

namespace server {

namespace {

// How far from the top and bottom edges each enemy's centre line must be, so
// that its whole flight (wave included) stays inside the arena.
constexpr float BydoMargin = game::bydo::HalfHeight + game::bydo::WaveAmplitude;
constexpr float PataPataMargin = game::patapata::HalfHeight + game::patapata::WaveAmplitude;
constexpr float BugMargin = game::bug::HalfHeight;
constexpr float CancerMargin = game::cancer::HalfHeight + game::cancer::WaveAmplitude;
constexpr float CruiserMargin = game::cruiser::HalfHeight;

// --- Single enemies --------------------------------------------------------------

/// An enemy that can arrive on its own: from level `unlock`, drawn with weight
/// `weight + growth × (level − unlock)`, so heavy enemies grow more common.
struct Single {
    SpawnFunction spawn;
    std::uint32_t unlock;
    int weight;
    int growth;
    float margin;
};

constexpr std::array Singles{
    Single{.spawn = spawnBydo, .unlock = 1, .weight = 40, .growth = 0, .margin = BydoMargin},
    Single{
        .spawn = spawnPataPata, .unlock = 1, .weight = 30, .growth = 0, .margin = PataPataMargin},
    Single{.spawn = spawnBug, .unlock = 2, .weight = 15, .growth = 4, .margin = BugMargin},
    Single{.spawn = spawnCancer, .unlock = 3, .weight = 10, .growth = 3, .margin = CancerMargin},
    Single{.spawn = spawnCruiser, .unlock = 4, .weight = 4, .growth = 2, .margin = CruiserMargin},
};

// --- Formations ------------------------------------------------------------------

/// One enemy of a formation: `dy` units below the formation's centre line,
/// entering `delay` seconds after the first one.
struct Member {
    SpawnFunction spawn;
    float dy;
    float delay;
};

/// Several enemies entering in a pattern, from level `unlock`. `margin` keeps
/// every member's flight inside the arena.
struct Formation {
    std::span<const Member> members;
    std::uint32_t unlock;
    float margin;
};

/// Five Pata-Patas in single file, following the same wave.
constexpr std::array PataPataLine{
    Member{.spawn = spawnPataPata, .dy = 0.0F, .delay = 0.0F},
    Member{.spawn = spawnPataPata, .dy = 0.0F, .delay = 0.25F},
    Member{.spawn = spawnPataPata, .dy = 0.0F, .delay = 0.5F},
    Member{.spawn = spawnPataPata, .dy = 0.0F, .delay = 0.75F},
    Member{.spawn = spawnPataPata, .dy = 0.0F, .delay = 1.0F},
};

/// Five Bydos in a V, the leader in front.
constexpr std::array BydoV{
    Member{.spawn = spawnBydo, .dy = 0.0F, .delay = 0.0F},
    Member{.spawn = spawnBydo, .dy = -70.0F, .delay = 0.35F},
    Member{.spawn = spawnBydo, .dy = 70.0F, .delay = 0.35F},
    Member{.spawn = spawnBydo, .dy = -140.0F, .delay = 0.7F},
    Member{.spawn = spawnBydo, .dy = 140.0F, .delay = 0.7F},
};

/// Four Bugs from different heights, all homing in on the ships.
constexpr std::array BugRush{
    Member{.spawn = spawnBug, .dy = -150.0F, .delay = 0.0F},
    Member{.spawn = spawnBug, .dy = 150.0F, .delay = 0.4F},
    Member{.spawn = spawnBug, .dy = -50.0F, .delay = 0.8F},
    Member{.spawn = spawnBug, .dy = 50.0F, .delay = 1.2F},
};

/// Two Cancers side by side: their fans cross.
constexpr std::array CancerPair{
    Member{.spawn = spawnCancer, .dy = -120.0F, .delay = 0.0F},
    Member{.spawn = spawnCancer, .dy = 120.0F, .delay = 0.0F},
};

/// A Cruiser escorted by two Cancers.
constexpr std::array Escort{
    Member{.spawn = spawnCruiser, .dy = 0.0F, .delay = 0.0F},
    Member{.spawn = spawnCancer, .dy = -150.0F, .delay = 0.6F},
    Member{.spawn = spawnCancer, .dy = 150.0F, .delay = 0.6F},
};

constexpr std::array Formations{
    Formation{.members = PataPataLine, .unlock = 1, .margin = PataPataMargin},
    Formation{.members = BydoV, .unlock = 2, .margin = BydoMargin + 140.0F},
    Formation{.members = BugRush, .unlock = 3, .margin = BugMargin + 150.0F},
    Formation{.members = CancerPair, .unlock = 4, .margin = CancerMargin + 120.0F},
    Formation{.members = Escort, .unlock = 5, .margin = CancerMargin + 150.0F},
};

// --- Choosing --------------------------------------------------------------------

/// A random height for a centre line `margin` units away from both edges.
float randomHeight(std::minstd_rand& random, float margin)
{
    std::uniform_real_distribution<float> height(margin, game::arena::Height - margin);
    return height(random);
}

/// Seconds before an armed enemy's first shot once it is on screen.
float randomFirstShot(std::minstd_rand& random)
{
    std::uniform_real_distribution<float> delay(0.5F, 2.0F);
    return delay(random);
}

/// Queues one enemy, unlocked at `level`. Returns how long the arrival lasts.
float queueSingle(WaveDirector& director, std::uint32_t level)
{
    std::array<int, Singles.size()> weights{};
    for (std::size_t i = 0; i < Singles.size(); ++i) {
        const Single& single = Singles.at(i);
        if (level >= single.unlock) {
            weights.at(i)
                = single.weight + (single.growth * static_cast<int>(level - single.unlock));
        }
    }
    std::discrete_distribution<std::size_t> pick(weights.begin(), weights.end());
    const Single& single = Singles.at(pick(director.random));
    const float y = randomHeight(director.random, single.margin);
    director.pending.push_back(PendingSpawn{
        .delay = 0.0F,
        .spawn = single.spawn,
        .y = y,
        .firstShotDelay = randomFirstShot(director.random),
    });
    return 0.0F;
}

/// Queues a formation unlocked at `level`. Returns how long the arrival lasts
/// (until its last member enters).
float queueFormation(WaveDirector& director, std::uint32_t level)
{
    std::vector<const Formation*> unlocked;
    for (const Formation& formation : Formations) {
        if (level >= formation.unlock) {
            unlocked.push_back(&formation);
        }
    }
    std::uniform_int_distribution<std::size_t> pick(0, unlocked.size() - 1);
    const Formation& formation = *unlocked.at(pick(director.random));
    const float centre = randomHeight(director.random, formation.margin);
    float duration = 0.0F;
    for (const Member& member : formation.members) {
        director.pending.push_back(PendingSpawn{
            .delay = member.delay,
            .spawn = member.spawn,
            .y = centre + member.dy,
            .firstShotDelay = randomFirstShot(director.random),
        });
        duration = std::max(duration, member.delay);
    }
    return duration;
}

/// Takes out of `pending` the enemies due now, after `deltaSeconds` more.
std::vector<PendingSpawn> takeDue(std::vector<PendingSpawn>& pending, float deltaSeconds)
{
    std::vector<PendingSpawn> due;
    for (PendingSpawn& spawn : pending) {
        spawn.delay -= deltaSeconds;
        if (spawn.delay <= 0.0F) {
            due.push_back(spawn);
        }
    }
    std::erase_if(pending, [](const PendingSpawn& spawn) { return spawn.delay <= 0.0F; });
    return due;
}

} // namespace

void WaveSystem::update(ecs::World& world, const ecs::Time& time)
{
    bool anyShip = false;
    world.query<PlayerShip>(
        [&](ecs::Entity /*entity*/, const PlayerShip& /*ship*/) { anyShip = true; });
    if (!anyShip) {
        return; // Nobody to fight: the waves wait
    }

    const std::uint32_t level = singleton<RunProgress>(world).level;
    auto& director = singleton<WaveDirector>(world);
    director.untilNext -= time.deltaSeconds();
    if (director.untilNext <= 0.0F) {
        std::bernoulli_distribution formation(
            static_cast<double>(game::waves::formationChance(level)));
        const float duration = formation(director.random) ? queueFormation(director, level)
                                                          : queueSingle(director, level);
        director.untilNext += duration + game::waves::spawnInterval(level);
    }

    // Created once the director is done with: spawning changes the World
    const std::vector<PendingSpawn> due = takeDue(director.pending, time.deltaSeconds());
    for (const PendingSpawn& spawn : due) {
        (void)spawn.spawn(world, spawn.y, spawn.firstShotDelay);
    }
}

} // namespace server
