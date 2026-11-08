#include "server/Systems/BossSystem.hpp"

#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Boss.hpp"
#include "game/Components/Health.hpp"
#include "game/Components/Motion.hpp"
#include "game/Enemies.hpp"
#include "game/Events/DespawnEvent.hpp"
#include "game/Run.hpp"
#include "network/Protocol/Protocol.hpp"
#include "server/Components/Behaviour.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/Match.hpp"
#include "server/Components/Player.hpp"
#include "server/Components/Waves.hpp"
#include "server/Singleton.hpp"
#include "server/Spawn.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace server {

namespace {

template <typename T>
bool any(ecs::World& world)
{
    bool found = false;
    world.query<T>([&](ecs::Entity /*entity*/, const T& /*component*/) { found = true; });
    return found;
}

BossAttack next(BossAttack attack) noexcept
{
    switch (attack) {
    case BossAttack::Fan:
        return BossAttack::Ring;
    case BossAttack::Ring:
        return BossAttack::Summon;
    case BossAttack::Summon:
        break;
    }
    return BossAttack::Fan;
}

/// The weapon of `attack`, reloading faster when `enraged`. Its cooldown
/// is kept, so swapping weapons never fires early.
void equip(Weapon& weapon, BossAttack attack, bool enraged, float timeLeft)
{
    const float cooldown = weapon.cooldown;
    weapon = attack == BossAttack::Ring ? weapons::BossRing : weapons::BossFan;
    if (enraged) {
        weapon.interval *= game::boss::EnragedFactor;
    }
    // Summon: no shot until the next attack
    weapon.cooldown = attack == BossAttack::Summon ? timeLeft : cooldown;
}

/// Health of the next boss: tougher after each defeat, one byte at most.
std::uint8_t nextBossHealth(std::uint32_t defeated) noexcept
{
    const std::uint32_t health = game::boss::BaseHealth + (game::boss::HealthPerDefeat * defeated);
    return static_cast<std::uint8_t>(std::min<std::uint32_t>(health, game::boss::MaxHealth));
}

} // namespace

BossSystem::BossSystem(ecs::World& world)
    : _world(world)
    , _onDespawn(world.events().subscribe<game::DespawnEvent>(
          [this](const game::DespawnEvent& despawn) { onDespawn(despawn); }))
{
}

void BossSystem::update(ecs::World& world, const ecs::Time& time)
{
    const RunProgress& progress = singleton<RunProgress>(world);
    if (!any<Boss>(world) && any<PlayerShip>(world) && progress.elapsed >= progress.nextBossAt
        && singleton<Match>(world).phase == network::protocol::GamePhase::Running) {
        (void)spawnBoss(world, game::arena::Height / 2.0F, nextBossHealth(progress.bossesDefeated));
    }

    std::vector<game::Position> summons; // Bugs to bring out, after the query
    world.query<Boss, AttackCycle, Weapon, game::Health, game::Position>(
        [&](ecs::Entity /*entity*/, const Boss& boss, AttackCycle& cycle, Weapon& weapon,
            const game::Health& health, const game::Position& position) {
            cycle.timeLeft -= time.deltaSeconds();
            if (cycle.timeLeft <= 0.0F) {
                cycle.current = next(cycle.current);
                cycle.timeLeft = game::boss::AttackSeconds;
                cycle.summoned = false;
                equip(weapon, cycle.current, cycle.enraged, cycle.timeLeft);
            }
            if (!cycle.enraged && health.current * 2 < boss.maxHealth) {
                cycle.enraged = true; // The rage starts at once, not at the next attack
                equip(weapon, cycle.current, true, cycle.timeLeft);
            }
            if (cycle.current == BossAttack::Summon && !cycle.summoned) {
                cycle.summoned = true;
                summons.push_back(position);
            }
        });

    constexpr std::array<float, game::boss::SummonCount> offsets{-150.0F, 0.0F, 150.0F};
    for (const game::Position& from : summons) {
        for (const float dy : offsets) {
            const ecs::Entity bug = spawnBug(world, from.y + dy, 0.0F);
            // Out of the boss's front, not from the right edge
            world.getComponent<game::Position>(bug) = {
                .x = from.x - game::boss::HalfWidth,
                .y = std::clamp(from.y + dy, game::bug::HalfHeight,
                    game::arena::Height - game::bug::HalfHeight),
            };
        }
    }
}

void BossSystem::onDespawn(const game::DespawnEvent& despawn)
{
    if (despawn.reason != game::DespawnReason::Killed
        || !_world.hasComponent<Boss>(despawn.entity)) {
        return;
    }
    auto& progress = singleton<RunProgress>(_world);
    ++progress.bossesDefeated;
    progress.nextBossAt = progress.elapsed + game::boss::Interval;
    auto& match = singleton<Match>(_world);
    match.lives = std::min(static_cast<std::uint8_t>(match.lives + 1), game::run::MaxLives);
}

} // namespace server
