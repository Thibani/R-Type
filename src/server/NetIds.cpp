#include "server/NetIds.hpp"

#include "ecs/World/World.hpp"
#include "network/Protocol/Protocol.hpp"
#include "server/Components/NetEntity.hpp"
#include "server/Singleton.hpp"

#include <stdexcept>
#include <unordered_set>

namespace server {

std::uint16_t allocateNetId(ecs::World& world)
{
    std::unordered_set<std::uint16_t> used;
    world.query<NetEntity>(
        [&](ecs::Entity /*entity*/, NetEntity& entity) { used.insert(entity.netId); });

    auto& counter = singleton<NetIdCounter>(world);
    // At most one full cycle: every id but NoEntity can be tried once
    for (std::uint32_t attempt = 0; attempt < network::protocol::NoEntity; ++attempt) {
        const std::uint16_t id = counter.next;
        counter.next
            = static_cast<std::uint16_t>(id + 1 == network::protocol::NoEntity ? 0 : id + 1);
        if (!used.contains(id)) {
            return id;
        }
    }
    throw std::runtime_error("allocateNetId: all 65535 network ids are in use");
}

} // namespace server
