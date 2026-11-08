#include "server/Systems/MatchSystem.hpp"

#include "ecs/World/World.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/Match.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/Player.hpp"
#include "server/Components/Waves.hpp"
#include "server/Run.hpp"
#include "server/Singleton.hpp"

namespace server {

using namespace network::protocol;

namespace {

template <typename T>
bool any(ecs::World& world)
{
    bool found = false;
    world.query<T>([&](ecs::Entity /*entity*/, const T& /*component*/) { found = true; });
    return found;
}

/// True if this game has started: someone played, scored or lost.
bool started(ecs::World& world)
{
    return singleton<Match>(world) != Match{} || singleton<RunProgress>(world).elapsed > 0.0F
        || singleton<TeamScore>(world).value > 0;
}

} // namespace

void MatchSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    if (!any<InGame>(world)) {
        if (started(world)) {
            startNewGame(world); // Everyone left: the next player starts afresh
        }
        return;
    }
    auto& match = singleton<Match>(world);
    if (match.phase == GamePhase::Running && match.lives == 0 && !any<PlayerShip>(world)
        && !any<RespawnTimer>(world)) {
        match.phase = GamePhase::GameOver;
    }
}

} // namespace server
