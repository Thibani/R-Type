#include "server/Systems/NetReceiveSystem.hpp"

#include "ecs/World/World.hpp"
#include "network/Protocol/PacketCodec.hpp"
#include "server/Components/ReceivedPacket.hpp"

#include <iostream>
#include <span>
#include <utility>

namespace server {

using namespace network::protocol;

NetReceiveSystem::NetReceiveSystem(network::UdpSocket& socket) noexcept
    : _socket(socket)
{
}

void NetReceiveSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    DatagramBuffer buffer{};

    for (std::size_t i = 0; i < MaxDatagramsPerFrame; ++i) {
        const auto received = _socket.receive(buffer);
        if (!received) {
            // The game must keep running: report and try again next frame
            std::cerr << "r-type_server: receive failed: " << received.error().message() << '\n';
            return;
        }
        if (!*received) {
            return; // Nothing left this frame
        }

        auto packet = decode(std::span(buffer).first((*received)->size), Direction::ClientToServer);
        if (!packet) {
            ++_dropped;
            continue;
        }
        const ecs::Entity entity = world.createEntity();
        world.addComponent(
            entity, ReceivedPacket{.from = (*received)->from, .packet = std::move(*packet)});
    }
}

std::size_t NetReceiveSystem::droppedCount() const noexcept
{
    return _dropped;
}

} // namespace server
