#pragma once

#include "ecs/Entity/Entity.hpp"
#include "ecs/Event/Subscription.hpp"
#include "ecs/System/System.hpp"

#include <unordered_set>

namespace ecs {
class World;
}

namespace game {

struct DespawnRequest;

/// The only way gameplay entities leave the game: turns each
/// `DespawnRequest` into one `DespawnEvent`, then destroys the entity.
///
/// What happens during a step:
///
///     LifetimeSystem / ArenaExitSystem / ...   emit DespawnRequest
///     events delivered:
///         DespawnSystem                        emits DespawnEvent,
///                                              records destroyEntity()
///         listeners of DespawnEvent            read the entity (still alive)
///     commands applied                         the entity is destroyed
///
/// So every listener of `DespawnEvent` sees the entity alive, whatever the
/// order of subscription, and each entity is announced exactly once even if
/// it was requested several times (only the first reason is kept). Requests
/// for entities that are already dead are ignored.
///
/// It reacts to events, so it needs the World when it is created:
///
///     world.systems().add<game::DespawnSystem>(world);
class DespawnSystem final : public ecs::System {
public:
    explicit DespawnSystem(ecs::World& world);

    /// Forgets the entities despawned during the previous step.
    void update(ecs::World& world, const ecs::Time& time) override;

private:
    void onRequest(const DespawnRequest& request);

    ecs::World& _world;
    std::unordered_set<ecs::Entity> _despawning; ///< Already announced this step
    ecs::Subscription _onRequest;
};

} // namespace game
