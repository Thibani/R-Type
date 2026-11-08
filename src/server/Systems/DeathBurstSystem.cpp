#include "server/Systems/DeathBurstSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "game/Events/DespawnEvent.hpp"
#include "network/Protocol/Protocol.hpp"
#include "server/Components/Behaviour.hpp"
#include "server/Spawn.hpp"

#include <cmath>
#include <cstdint>
#include <numbers>

namespace server {

DeathBurstSystem::DeathBurstSystem(ecs::World& world)
    : _world(world)
    , _onDespawn(world.events().subscribe<game::DespawnEvent>(
          [this](const game::DespawnEvent& despawn) { onDespawn(despawn); }))
{
}

void DeathBurstSystem::update(ecs::World& /*world*/, const ecs::Time& /*time*/) { }

void DeathBurstSystem::onDespawn(const game::DespawnEvent& despawn)
{
    if (despawn.reason != game::DespawnReason::Killed) {
        return;
    }
    const auto* burst = _world.tryGetComponent<BurstOnDeath>(despawn.entity);
    const auto* position = _world.tryGetComponent<game::Position>(despawn.entity);
    if (burst == nullptr || position == nullptr || burst->shots == 0) {
        return;
    }
    // Copied: creating entities below may move the components in memory
    const BurstOnDeath ring = *burst;
    const game::Position from = *position;
    const float step = 2.0F * std::numbers::pi_v<float> / static_cast<float>(ring.shots);
    for (std::uint8_t i = 0; i < ring.shots; ++i) {
        const float angle = step * static_cast<float>(i);
        // Events are delivered outside of any query: entities can be created here
        (void)spawnProjectile(_world, ring.projectile, network::protocol::NoPlayer, from,
            {.dx = std::cos(angle), .dy = std::sin(angle)});
    }
}

} // namespace server
