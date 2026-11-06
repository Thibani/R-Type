#pragma once

#include "ecs/World/World.hpp"
#include "network/Protocol/Packets.hpp"
#include "network/Reliability/ReliableChannel.hpp"
#include "server/Components/NetClient.hpp"

#include <map>
#include <string>

namespace server {

/// The channels of the players online this frame, by name. A session that is
/// leaving no longer counts, from the frame it leaves: a new login of the same
/// account can come in the next frame, and must count as an arrival.
class OnlinePlayers {
public:
    explicit OnlinePlayers(ecs::World& world)
    {
        world.query<NetClient, LoggedIn>(
            [&](ecs::Entity entity, NetClient& client, LoggedIn& /*loggedIn*/) {
                if (!world.hasComponent<Disconnecting>(entity)) {
                    _channels.emplace(client.name, &client.channel);
                }
            });
    }

    [[nodiscard]]
    bool contains(const std::string& name) const
    {
        return _channels.contains(name);
    }

    /// Nothing if `name` is offline.
    void tell(const std::string& name, const network::protocol::Payload& payload) const
    {
        if (const auto found = _channels.find(name); found != _channels.end()) {
            found->second->queueReliable(payload);
        }
    }

    [[nodiscard]]
    const std::map<std::string, network::ReliableChannel*>& channels() const noexcept
    {
        return _channels;
    }

private:
    std::map<std::string, network::ReliableChannel*> _channels;
};

} // namespace server
