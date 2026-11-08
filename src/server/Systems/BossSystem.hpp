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

/// Brings the boss and plays its attacks (see `game/Boss.hpp`). Runs in the
/// simulation, after `ProgressSystem` and before `AimSystem`.
///
///  - Arrival: once `RunProgress::elapsed` reaches `nextBossAt`, while a ship
///    is in the game and no boss is there, a boss flies in (`spawnBoss`),
///    tougher after each defeat. `WaveSystem` sends no new wave meanwhile.
///  - Attacks: every `boss::AttackSeconds`, the next of Fan → Ring → Summon:
///    its `Weapon` is swapped (Summon: no shot, 3 Bugs come out of it). Below
///    half health, its weapons reload faster.
///  - Defeat (`DespawnEvent`, KILLED): one life back for the team (up to
///    `run::MaxLives`) and the next boss `boss::Interval` seconds later. Its
///    points come with its `ScoreValue`, like any enemy.
///
///     world.systems().add<BossSystem>(world);
class BossSystem final : public ecs::System {
public:
    explicit BossSystem(ecs::World& world);

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    void onDespawn(const game::DespawnEvent& despawn);

    ecs::World& _world;
    ecs::Subscription _onDespawn;
};

} // namespace server
