#include "game/Systems/DespawnSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Events/DespawnEvent.hpp"

namespace game {

DespawnSystem::DespawnSystem(ecs::World& world)
    : _world(world)
    , _onRequest(world.events().subscribe<DespawnRequest>(
          [this](const DespawnRequest& request) { onRequest(request); }))
{
}

void DespawnSystem::update(ecs::World& /*world*/, const ecs::Time& /*time*/)
{
    // The entities announced last step are destroyed by now
    _despawning.clear();
}

void DespawnSystem::onRequest(const DespawnRequest& request)
{
    if (!_world.isAlive(request.entity) || !_despawning.insert(request.entity).second) {
        return; // Already gone, or already announced this step
    }
    // Delivered in this same dispatch, before the commands below are applied
    _world.events().emit(
        DespawnEvent{.entity = request.entity, .reason = request.reason, .killer = request.killer});
    _world.commands().destroyEntity(request.entity);
}

} // namespace game
