#include "game/Systems/LifetimeSystem.hpp"

#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Lifetime.hpp"
#include "game/Events/DespawnEvent.hpp"

namespace game {

void LifetimeSystem::update(ecs::World& world, const ecs::Time& time)
{
    const float deltaSeconds = time.deltaSeconds();

    world.query<Lifetime>([&](ecs::Entity entity, Lifetime& lifetime) {
        lifetime.remainingSeconds -= deltaSeconds;
        if (lifetime.remainingSeconds <= 0.0F) {
            world.events().emit(DespawnRequest{
                .entity = entity, .reason = DespawnReason::Expired, .killer = ecs::NullEntity});
        }
    });
}

} // namespace game
