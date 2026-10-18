#include "server/Network.hpp"

#include "ecs/World/World.hpp"
#include "network/Protocol/PacketCodec.hpp"
#include "server/Components/NetClient.hpp"

#include <chrono>
#include <iostream>
#include <span>

namespace server {

using namespace network::protocol;

TimePoint toTimePoint(const ecs::Time& time) noexcept
{
    const std::chrono::duration<double> elapsed(static_cast<double>(time.elapsedSeconds()));
    return TimePoint{} + std::chrono::duration_cast<Clock::duration>(elapsed);
}

void sendPacket(network::NetworkThread& socket, const Packet& packet, const network::Endpoint& to)
{
    DatagramBuffer buffer{};
    const auto size = encode(packet, buffer);
    if (!size) {
        // A bug in the server, not in the network: our own packet is invalid
        std::cerr << "r-type_server: cannot encode packet: " << toString(size.error()) << '\n';
        return;
    }
    socket.send(std::span(buffer).first(*size), to); // Sent by the network thread
}

void notifyShutdown(ecs::World& world, network::NetworkThread& socket, TimePoint now)
{
    world.query<NetClient>([&](ecs::Entity /*entity*/, NetClient& client) {
        for (int i = 0; i < 3; ++i) { // No acknowledgement: repeat instead (§8.3)
            const Packet packet
                = client.channel.wrap(Disconnect{.reason = DisconnectReason::ServerShutdown}, now);
            sendPacket(socket, packet, client.endpoint);
        }
    });
}

} // namespace server
