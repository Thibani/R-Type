#pragma once

#include "ecs/Event/Subscription.hpp"
#include "ecs/System/System.hpp"

namespace ecs {
class World;
} // namespace ecs

namespace game {
struct DespawnEvent;
} // namespace game

namespace server {

/// What a destroyed ship costs (see `game/Run.hpp`): when a ship is killed
/// (`DespawnEvent`, KILLED), the team loses one life. If it has lives left,
/// the ship's player gets a `RespawnTimer` and a new ship after the delay;
/// otherwise the player is `Eliminated` until the next game.
///
/// It reacts to events, so it needs the World when it is created:
///
///     world.systems().add<LivesSystem>(world);
class LivesSystem final : public ecs::System {
public:
    explicit LivesSystem(ecs::World& world);

    /// Nothing to do each step: everything happens when a ship is killed.
    void update(ecs::World& world, const ecs::Time& time) override;

private:
    void onDespawn(const game::DespawnEvent& despawn);

    ecs::World& _world;
    ecs::Subscription _onDespawn;
};

} // namespace server
