#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Keeps one ship per connected player: creates the ship of a player who has
/// none, and destroys the ships whose player left. Runs at the end of
/// `frameStart()`, after the sessions were updated.
///
/// A ship is its own entity (see `spawnPlayerShip`): `PlayerShip`,
/// `Position`, `Velocity`, `Hitbox`, `Health` (3), `ConfinedToArena`,
/// `Weapon`, `Trigger`, `NetEntity`. It is not the player's entity, because a
/// ship can die while its player stays connected (#59).
///
/// A ship killed in combat (#59) comes back after `ship::RespawnDelay`
/// seconds: `DamageSystem` gives the player a `RespawnTimer`, counted down here.
///
/// shortcut: no lives and no game over yet, ships come back forever; the
/// game phases (#110) will end the match when every ship is dead.
class PlayerShipSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
