#pragma once

#include "ecs/System/System.hpp"
#include "network/Protocol/Packets.hpp"
#include "network/Transport/NetworkThread.hpp"
#include "server/Systems/ConnectionSystem.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace server {

/// Sends every client the state of every networked entity, at the snapshot
/// rate (30 Hz, §9): one SNAPSHOT, unreliable, split in parts of at most 89
/// entities (§4.3). Runs last in `frameEnd()`, after `NetSendSystem`, so the
/// ENTITY_SPAWN of a new entity leaves before its first snapshot.
///
/// A snapshot is due when 2 simulation steps (60 Hz / 30 Hz) went by since
/// the last one: a frame without a step sends nothing new.
///
/// shortcut: `health` is always `ship::MaxHealth` (3), only ships exist; a
/// `Health` component that damage lowers comes with damage and death (#59).
class SnapshotSystem final : public ecs::System {
public:
    static constexpr std::uint32_t TicksPerSnapshot
        = ConnectionSystem::TickRate / ConnectionSystem::SnapshotRate;

    explicit SnapshotSystem(network::NetworkThread& socket) noexcept;

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    network::NetworkThread& _socket;
};

/// Splits `entities` into the parts of one snapshot (at least one part, even
/// with no entity, so the client still gets `lastInputTick`).
[[nodiscard]]
std::vector<network::protocol::Snapshot> snapshotParts(
    std::span<const network::protocol::EntityState> entities, std::uint32_t serverTick,
    std::uint32_t lastInputTick);

} // namespace server
