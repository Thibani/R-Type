#include "server/Systems/SnapshotSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "game/Ship.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/NetEntity.hpp"
#include "server/Components/Player.hpp"
#include "server/Network.hpp"
#include "server/Singleton.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>

namespace server {

using namespace network::protocol;

std::vector<Snapshot> snapshotParts(
    std::span<const EntityState> entities, std::uint32_t serverTick, std::uint32_t lastInputTick)
{
    // partCount is a u8: beyond 255 full parts, the rest is not sent
    constexpr std::size_t maxEntities = std::numeric_limits<std::uint8_t>::max() * MaxSnapshotEntities;
    entities = entities.first(std::min(entities.size(), maxEntities));

    const std::size_t partCount
        = std::max<std::size_t>(1, (entities.size() + MaxSnapshotEntities - 1) / MaxSnapshotEntities);
    std::vector<Snapshot> parts;
    parts.reserve(partCount);
    for (std::size_t part = 0; part < partCount; ++part) {
        const auto first = entities.subspan(std::min(part * MaxSnapshotEntities, entities.size()));
        const auto these = first.first(std::min(first.size(), MaxSnapshotEntities));
        parts.push_back(Snapshot{
            .serverTick = serverTick,
            .lastInputTick = lastInputTick,
            .partIndex = static_cast<std::uint8_t>(part),
            .partCount = static_cast<std::uint8_t>(partCount),
            .entities = {these.begin(), these.end()},
        });
    }
    return parts;
}

SnapshotSystem::SnapshotSystem(network::NetworkThread& socket) noexcept
    : _socket(socket)
{
}

void SnapshotSystem::update(ecs::World& world, const ecs::Time& time)
{
    const std::uint32_t tick = singleton<SimulationTick>(world).value;
    auto& last = singleton<LastSnapshot>(world).tick;
    if (last && tick - *last < TicksPerSnapshot) {
        return;
    }
    last = tick;

    std::vector<EntityState> entities;
    world.query<NetEntity, game::Position, game::Velocity>([&](ecs::Entity /*entity*/,
                                                               NetEntity& net,
                                                               game::Position& position,
                                                               game::Velocity& velocity) {
        entities.push_back(EntityState{
            .netId = net.netId,
            .kind = net.kind,
            .owner = net.owner,
            .health = game::ship::MaxHealth,
            .x = quantize(position.x),
            .y = quantize(position.y),
            .vx = quantize(velocity.dx),
            .vy = quantize(velocity.dy),
        });
    });

    const TimePoint now = toTimePoint(time);
    world.query<NetClient>([&](ecs::Entity entity, NetClient& client) {
        const auto* input = world.tryGetComponent<PlayerInput>(entity);
        const std::uint32_t lastInputTick = input != nullptr ? input->lastTick : 0;
        for (Snapshot& part : snapshotParts(entities, tick, lastInputTick)) {
            sendPacket(_socket, client.channel.wrap(std::move(part), now), client.endpoint);
        }
        client.lastSent = now;
    });
}

} // namespace server
