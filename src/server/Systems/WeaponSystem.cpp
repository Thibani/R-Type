#include "server/Systems/WeaponSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/NetEntity.hpp"
#include "server/Spawn.hpp"

#include <cstdint>
#include <vector>

namespace server {

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
        shots.push_back(Shot{
            .projectile = weapon.projectile,
            .owner = net.owner,
            .from = {.x = position.x + weapon.muzzle.x, .y = position.y + weapon.muzzle.y},
            .direction = trigger.direction,
        });
    });
    for (const Shot& shot : shots) {
        (void)spawnProjectile(world, shot.projectile, shot.owner, shot.from, shot.direction);
    }
}

} // namespace server
