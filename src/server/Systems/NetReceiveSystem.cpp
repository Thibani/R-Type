#include "server/Systems/NetReceiveSystem.hpp"

#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "network/Crypto/SealedDatagram.hpp"
#include "network/Protocol/PacketCodec.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/ReceivedPacket.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace server {

using namespace network::protocol;

namespace {

/// Size of the CONNECT_REJECT answer: replying to a smaller datagram would
/// let a spoofed request amplify traffic toward its victim (§7.1).
constexpr std::size_t RejectDatagramSize = HeaderSize + 1;

/// The session this datagram claims to belong to. Tokens are never 0, so a
/// CONNECT_REQUEST never has one.
NetClient* findSession(ecs::World& world, const network::Endpoint& from, std::uint32_t token)
{
    NetClient* found = nullptr;
    world.query<NetClient>([&](ecs::Entity /*entity*/, NetClient& client) {
        if (client.endpoint == from && client.token == token) {
            found = &client;
        }
    });
    return found;
}

} // namespace

NetReceiveSystem::NetReceiveSystem(network::NetworkThread& socket) noexcept
    : _socket(socket)
{
}

void NetReceiveSystem::update(ecs::World& world, const ecs::Time& time)
{
    // At most NetworkThread::MaxQueuedDatagrams: a flood cannot stall a frame
    for (const network::ReceivedDatagram& received : _socket.takeReceived()) {
        const network::Endpoint& from = received.from;
        if (!withinRate(from, time.elapsedSeconds())) {
            ++_rateLimited;
            continue;
        }

        std::span<const std::byte> datagram = received.bytes;
        const auto sealedHeader = decodeHeader(datagram);
        NetClient* session
            = sealedHeader ? findSession(world, from, sealedHeader->sessionToken) : nullptr;
        DatagramBuffer opened{};
        if (session != nullptr) {
            const auto size = network::crypto::openDatagram(session->cipher, datagram, opened);
            if (!size) {
                ++_dropped;
                continue;
            }
            datagram = std::span(opened).first(*size);
        }

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
