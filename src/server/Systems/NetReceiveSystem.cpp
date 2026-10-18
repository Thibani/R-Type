#include "server/Systems/NetReceiveSystem.hpp"

#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "network/Protocol/PacketCodec.hpp"
#include "server/Components/ReceivedPacket.hpp"

#include <iostream>
#include <span>
#include <utility>

namespace server {

using namespace network::protocol;

namespace {

/// Size of the CONNECT_REJECT answer: replying to a smaller datagram would
/// let a spoofed request amplify traffic toward its victim (§7.1).
constexpr std::size_t RejectDatagramSize = HeaderSize + 1;

} // namespace

NetReceiveSystem::NetReceiveSystem(network::UdpSocket& socket) noexcept
    : _socket(socket)
{
}

void NetReceiveSystem::update(ecs::World& world, const ecs::Time& time)
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
        const network::Endpoint& from = (*received)->from;
        if (!withinRate(from, time.elapsedSeconds())) {
            ++_rateLimited;
            continue;
        }

        const auto datagram = std::span(buffer).first((*received)->size);
        auto packet = decode(datagram, Direction::ClientToServer);
        if (!packet) {
            ++_dropped;
            const auto header = decodeHeader(datagram);
            if (packet.error() == DecodeError::UnsupportedVersion && header
                && header->type == static_cast<std::uint8_t>(PacketType::ConnectRequest)
                && datagram.size() >= RejectDatagramSize) {
                world.addComponent(world.createEntity(), IncompatibleClient{.from = from});
            }
            continue;
        }
        world.addComponent(
            world.createEntity(), ReceivedPacket{.from = from, .packet = std::move(*packet)});
    }
}

bool NetReceiveSystem::withinRate(const network::Endpoint& from, float now)
{
    if (now - _windowStart >= 1.0F) {
        _packetsThisSecond.clear();
        _windowStart = now;
    }
    return ++_packetsThisSecond[from] <= MaxPacketsPerSecond;
}

std::size_t NetReceiveSystem::droppedCount() const noexcept
{
    return _dropped;
}

std::size_t NetReceiveSystem::rateLimitedCount() const noexcept
{
    return _rateLimited;
}

} // namespace server
