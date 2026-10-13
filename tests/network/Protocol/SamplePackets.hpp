#pragma once

#include "network/Protocol/PacketCodec.hpp"
#include "network/Protocol/Packets.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <span>
#include <utility>
#include <vector>

/// One valid packet of every type, shared by the codec tests.
namespace samples {

using namespace network::protocol;

struct Sample {
    Packet packet;
    Direction direction; // Direction a receiver decodes it with
};

inline Packet makePacket(Payload payload, std::uint16_t reliableId = 0)
{
    return Packet{
        .sessionToken = 0xCAFEBABE,
        .sequence = 42,
        .ack = Ack{.sequence = 41, .bits = 0x80000001},
        .reliableId = reliableId,
        .payload = std::move(payload),
    };
}

inline std::vector<Sample> all()
{
    Snapshot snapshot{
        .serverTick = 1000,
        .lastInputTick = 990,
        .partIndex = 1,
        .partCount = 2,
        .entities = {},
    };
    snapshot.entities.push_back(EntityState{
        .netId = 1,
        .kind = EntityKind::PlayerShip,
        .owner = 0,
        .health = 3,
        .x = quantize(100.0F),
        .y = quantize(540.0F),
        .vx = quantize(250.0F),
        .vy = quantize(-125.5F),
    });
    snapshot.entities.push_back(EntityState{
        .netId = 77,
        .kind = EntityKind::Bydo,
        .owner = NoPlayer,
        .health = 1,
        .x = quantize(1950.0F),
        .y = quantize(300.25F),
        .vx = quantize(-80.0F),
        .vy = 0,
    });

    Input input{.firstTick = 500, .count = 3, .masks = {}};
    input.masks[0] = input::Up;
    input.masks[1] = input::Up | input::Fire;
    input.masks[2] = 0;

    const EntitySpawn spawn{
        .netId = 12,
        .kind = EntityKind::PlayerMissile,
        .owner = 1,
        .x = quantize(120.0F),
        .y = quantize(200.0F),
        .vx = quantize(900.0F),
        .vy = 0,
    };
    const EntityDestroy destroy{.netId = 77, .reason = DestroyReason::Killed, .killerNetId = 12};
    const ConnectAccept accept{.playerId = 2, .tickRate = 60, .snapshotRate = 30, .maxPlayers = 4};
    const Snapshot emptySnapshot{
        .serverTick = 1,
        .lastInputTick = 0,
        .partIndex = 0,
        .partCount = 1,
        .entities = {},
    };

    const auto toServer = [](Packet packet) {
        return Sample{.packet = std::move(packet), .direction = Direction::ClientToServer};
    };
    const auto toClient = [](Packet packet) {
        return Sample{.packet = std::move(packet), .direction = Direction::ServerToClient};
    };

    return {
        toServer(makePacket(ConnectRequest{.name = "Thibani"})),
        toClient(makePacket(accept)),
        toClient(makePacket(ConnectReject{.reason = RejectReason::GameInProgress})),
        toServer(makePacket(Disconnect{.reason = DisconnectReason::Timeout})),
        toClient(makePacket(Disconnect{.reason = DisconnectReason::ServerShutdown})),
        toServer(makePacket(Heartbeat{})),
        toServer(makePacket(input)),
        toClient(makePacket(snapshot)),
        toClient(makePacket(emptySnapshot)),
        toClient(makePacket(spawn, 7)),
        toClient(makePacket(destroy, 8)),
        toClient(makePacket(PlayerJoined{.playerId = 3, .name = "Zoë"}, 9)),
        toClient(makePacket(PlayerLeft{.playerId = 1, .reason = LeaveReason::Timeout}, 10)),
        toClient(makePacket(PlayerDied{.playerId = 0, .killerNetId = NoEntity}, 11)),
        toClient(makePacket(GameState{.phase = GamePhase::Running, .score = 123456}, 65535)),
    };
}

/// Encodes `packet`, failing the current test if encoding fails.
inline std::vector<std::byte> encodeOrFail(const Packet& packet)
{
    DatagramBuffer buffer{};
    const auto size = encode(packet, buffer);
    EXPECT_TRUE(size.has_value()) << (size ? "" : toString(size.error()));
    if (!size) {
        return {};
    }
    const auto written = std::span(buffer).first(*size);
    return {written.begin(), written.end()};
}

} // namespace samples
