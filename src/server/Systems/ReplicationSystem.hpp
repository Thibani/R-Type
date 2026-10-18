#pragma once

#include "ecs/System/System.hpp"

namespace server {

/// Tells every client which networked entities exist (docs/protocol.md
/// §4.3): ENTITY_SPAWN for each `NetEntity` the client was not told about,
/// ENTITY_DESTROY for each one it knows that is gone. Runs in `frameEnd()`,
/// after `SessionCleanupSystem` and before `NetSendSystem`.
///
/// Each `NetClient` remembers the netIds it was sent (`knownEntities`), so a
/// newcomer gets one spawn per existing entity (§8.1) with the same code that
/// announces new entities to everyone. The events follow the PLAYER_JOINED
/// queued in `frameStart()`, as §8.1 orders.
///
/// shortcut: every destroy says KILLED by no one; the real reason and killer
/// come with damage and death (#59).
class ReplicationSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
