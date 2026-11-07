#include "server/Systems/DropSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "game/Events/DespawnEvent.hpp"
#include "game/Gadgets.hpp"
#include "server/Components/Behaviour.hpp"
#include "server/Components/Gadgets.hpp"
#include "server/Singleton.hpp"
#include "server/Spawn.hpp"

#include <random>

namespace server {

DropSystem::DropSystem(ecs::World& world)
    : _world(world)
    , _onDespawn(world.events().subscribe<game::DespawnEvent>(
          [this](const game::DespawnEvent& despawn) { onDespawn(despawn); }))
{
}

void DropSystem::update(ecs::World& /*world*/, const ecs::Time& /*time*/) { }

void DropSystem::onDespawn(const game::DespawnEvent& despawn)
{
    if (despawn.reason != game::DespawnReason::Killed
        || !_world.hasComponent<Enemy>(despawn.entity)) {
        return;
    }
    const auto* position = _world.tryGetComponent<game::Position>(despawn.entity);
    if (position == nullptr) {
        return;
    }
    const game::Position at = *position; // Copied: spawning may move components

    // Events are delivered outside of any query: entities can be created here
    if (_world.hasComponent<Boss>(despawn.entity)) {
        constexpr float apart = 60.0F; // Side by side, so both can be seen
        (void)spawnPickup(_world, PickupKind::Repair, {.x = at.x, .y = at.y - apart});
        (void)spawnPickup(_world, PickupKind::Force, {.x = at.x, .y = at.y + apart});
        return;
    }
    std::uniform_real_distribution<float> dice(0.0F, 1.0F);
    const float roll = dice(singleton<DropTable>(_world).random);
    if (roll < game::pickup::RepairChance) {
        (void)spawnPickup(_world, PickupKind::Repair, at);
    } else if (roll < game::pickup::RepairChance + game::pickup::ForceChance) {
        (void)spawnPickup(_world, PickupKind::Force, at);
    }
}

} // namespace server
