#include "server/Systems/WeaponSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/NetEntity.hpp"
#include "server/Spawn.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace server {

namespace {

/// `direction` turned by `radians` (positive: clockwise on screen, y is down).
game::Velocity rotated(const game::Velocity& direction, float radians) noexcept
{
    const float cos = std::cos(radians);
    const float sin = std::sin(radians);
    return {
        .dx = (direction.dx * cos) - (direction.dy * sin),
        .dy = (direction.dx * sin) + (direction.dy * cos),
    };
}

} // namespace

void WeaponSystem::update(ecs::World& world, const ecs::Time& time)
{
    struct Shot {
        Projectile projectile;
        std::uint8_t owner;
        game::Position from;
        game::Velocity direction;
    };
    std::vector<Shot> shots; // Created after the query
    world.query<Weapon, Trigger, game::Position, NetEntity>([&](ecs::Entity /*entity*/,
                                                                Weapon& weapon, Trigger& trigger,
                                                                game::Position& position,
                                                                NetEntity& net) {
        weapon.cooldown -= time.deltaSeconds();
        if (!trigger.pulled || weapon.cooldown > 0.0F) {
            return;
        }
        weapon.cooldown = weapon.interval;
        const game::Position from{
            .x = position.x + weapon.muzzle.x, .y = position.y + weapon.muzzle.y};
        // The fan is centred on the trigger's direction: offsets -1, 0, +1 for 3 shots
        const float middle = static_cast<float>(weapon.shots - 1) / 2.0F;
        for (std::uint8_t i = 0; i < weapon.shots; ++i) {
            shots.push_back(Shot{
                .projectile = weapon.projectile,
                .owner = net.owner,
                .from = from,
                .direction
                = rotated(trigger.direction, (static_cast<float>(i) - middle) * weapon.spread),
            });
        }
    });
    for (const Shot& shot : shots) {
        (void)spawnProjectile(world, shot.projectile, shot.owner, shot.from, shot.direction);
    }
}

} // namespace server
