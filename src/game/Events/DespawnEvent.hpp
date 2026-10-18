#pragma once

#include "ecs/Entity/Entity.hpp"

#include <cstdint>

namespace game {

/// Why an entity leaves the game.
///
/// The values are the `reason` byte of ENTITY_DESTROY (docs/protocol.md §5),
/// so the server can send them as they are.
enum class DespawnReason : std::uint8_t {
    Killed = 0, ///< Destroyed by a hit (damage and death systems)
    OutOfBounds = 1, ///< Left the arena (`ArenaExitSystem`)
    Expired = 2, ///< Its `Lifetime` ran out (`LifetimeSystem`)
};

/// Asks for an entity to be removed from the game.
///
/// Systems never destroy gameplay entities themselves: they emit this request
/// and `DespawnSystem` does the rest. Several requests for the same entity in
/// the same step (a missile that expires as it leaves the screen) are fine:
/// only the first one counts.
struct DespawnRequest {
    ecs::Entity entity;
    DespawnReason reason{DespawnReason::Killed};
    ecs::Entity killer; ///< Who is responsible, `ecs::NullEntity` if no one
};

/// An entity is being removed from the game. Emitted by `DespawnSystem`,
/// once per entity.
///
/// The entity is **still alive** while this event is delivered: listeners
/// can read its components (the server its netId, the client its sprite).
/// It is destroyed right after the events of the step have been delivered.
struct DespawnEvent {
    ecs::Entity entity;
    DespawnReason reason{DespawnReason::Killed};
    ecs::Entity killer; ///< Who is responsible, `ecs::NullEntity` if no one
};

} // namespace game
