#include "server/Run.hpp"

#include "ecs/World/World.hpp"
#include "game/Boss.hpp"
#include "game/Events/DespawnEvent.hpp"
#include "game/Waves.hpp"
#include "server/Components/Behaviour.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/Match.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/NetEntity.hpp"
#include "server/Components/Player.hpp"
#include "server/Components/Waves.hpp"
#include "server/Singleton.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace server {

using namespace network::protocol;

namespace {

/// How far the run is from the next boss, as GAME_STATE says it: 0-99 %
/// on the way, 100 while a boss is there.
std::uint8_t bossProgress(ecs::World& world)
{
    bool bossHere = false;
    world.query<Boss>([&](ecs::Entity /*entity*/, const Boss& /*boss*/) { bossHere = true; });
    if (bossHere) {
        return MaxBossProgress;
    }
    const RunProgress& progress = singleton<RunProgress>(world);
    const float since = progress.elapsed - (progress.nextBossAt - game::boss::Interval);
    const float percent = 100.0F * since / game::boss::Interval;
    return static_cast<std::uint8_t>(std::clamp(percent, 0.0F, 99.0F));
}

} // namespace

GameState currentState(ecs::World& world)
{
    const std::uint8_t boss = bossProgress(world);
    const Match& match = singleton<Match>(world);
    const std::uint32_t level = singleton<RunProgress>(world).level;
    return GameState{
        .phase = match.phase,
        .score = singleton<TeamScore>(world).value,
        // One byte on the wire: level 255 is 2 hours of play
        .level = static_cast<std::uint8_t>(
            std::min<std::uint32_t>(level, std::numeric_limits<std::uint8_t>::max())),
        .lives = match.lives,
        .bossProgress = boss,
    };
}

void startNewGame(ecs::World& world)
{
    singleton<TeamScore>(world) = {};
    singleton<RunProgress>(world) = {};
    singleton<Match>(world) = {};
    auto& director = singleton<WaveDirector>(world);
    director.pending.clear(); // The random source goes on: the next game differs
    director.untilNext = game::waves::BaseSpawnInterval;

    std::vector<ecs::Entity> players;
    world.query<NetClient>(
        [&](ecs::Entity entity, const NetClient& /*client*/) { players.push_back(entity); });
    for (const ecs::Entity player : players) {
        world.removeComponent<Eliminated>(player);
        world.removeComponent<RespawnTimer>(player);
    }

    world.query<NetEntity>([&](ecs::Entity entity, const NetEntity& /*net*/) {
        if (!world.hasComponent<PlayerShip>(entity)) {
            world.events().emit(game::DespawnRequest{.entity = entity,
                .reason = game::DespawnReason::Expired,
                .killer = ecs::NullEntity});
        }
    });
}

} // namespace server
