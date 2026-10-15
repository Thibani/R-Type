#pragma once

#include "network/Protocol/Packets.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <map>
#include <optional>
#include <vector>

/// Sequence numbers, acknowledgements and reliable messages (docs/protocol.md §6).
namespace network {

/// True if sequence `a` is newer than `b`, across the 65535 → 0 wrap-around
/// (RFC 1982 serial arithmetic, §6.1).
[[nodiscard]]
constexpr bool isNewer(std::uint16_t a, std::uint16_t b) noexcept
{
    constexpr std::uint16_t half = 32768;
    return (a > b && a - b <= half) || (a < b && b - a > half);
}

enum class ChannelError : std::uint8_t {
    ReliableBufferFull, // The peer sent too many reliable messages out of order (§6.2)
};

/// State of one connection, on one side: one per client on the server, one
/// on the client. It never touches a socket and never reads a clock: the
/// caller sends what it returns and passes the current time.
///
///     // Sending
///     socket.send(channel.wrap(Snapshot{...}, now));    // unreliable
///     channel.queueReliable(EntitySpawn{...});            // reliable
///     for (const Packet& packet : channel.takeDue(now)) { // new + resends
///         socket.send(packet);
///     }
///     if (channel.timedOut(now)) { /* peer is gone */ }
///
///     // Receiving (already decoded and checked against the session)
///     auto payloads = channel.receive(packet, now);
///     if (!payloads) { /* protocol error: disconnect */ }
///     for (const Payload& payload : *payloads) { /* in order, no duplicate */ }
///
/// What it guarantees:
///  - every packet sent carries a new sequence and acknowledges what was received;
///  - duplicate packets and packets more than 32 sequences old are dropped;
///  - reliable messages are resent until acknowledged, and delivered exactly
///    once, in the order they were queued.
class ReliableChannel {
public:
    using Clock = std::chrono::steady_clock;

    static constexpr std::chrono::milliseconds MinResendDelay{100}; // §6.2: max(100 ms, 2 × RTT)
    static constexpr std::chrono::seconds ReliableTimeout{5}; // §6.2: then the peer is gone
    static constexpr std::size_t MaxBufferedReliable = 64; // §6.2
    static constexpr std::uint16_t AckWindow = 32; // §6.1: bits of ackBits

    explicit ReliableChannel(std::uint32_t sessionToken = 0) noexcept;

    /// Token put in every packet (0 until the client receives CONNECT_ACCEPT).
    void setSessionToken(std::uint32_t sessionToken) noexcept;

    // --- Sending ----------------------------------------------------------------

    /// Builds a packet for an unreliable payload (HEARTBEAT, INPUT, SNAPSHOT...).
    [[nodiscard]]
    protocol::Packet wrap(protocol::Payload payload, Clock::time_point now);

    /// Queues a reliable payload (ENTITY_SPAWN, PLAYER_JOINED...). It is sent
    /// by the next `takeDue()`, then again until acknowledged.
    void queueReliable(protocol::Payload payload);

    /// Packets to send now: reliable messages never sent, and those not
    /// acknowledged within the resend delay. Each gets a new sequence.
    /// Messages more than 64 ids ahead of the oldest unacknowledged one wait,
    /// so the peer's reorder buffer can never overflow.
    [[nodiscard]]
    std::vector<protocol::Packet> takeDue(Clock::time_point now);

    /// True once a reliable message stayed unacknowledged for 5 seconds.
    [[nodiscard]]
    bool timedOut(Clock::time_point now) const noexcept;

    // --- Receiving --------------------------------------------------------------

    /// Processes a packet from the peer: its acknowledgements, then its payload.
    /// Returns the payloads to handle now, in order: none for a duplicate, an
    /// old packet or an early reliable message, several when an early one
    /// unblocks the ones buffered after it.
    [[nodiscard]]
    std::expected<std::vector<protocol::Payload>, ChannelError> receive(
        const protocol::Packet& packet, Clock::time_point now);

    // --- Statistics -------------------------------------------------------------

    /// Smoothed round-trip time, or nullopt before the first acknowledgement.
    [[nodiscard]]
    std::optional<Clock::duration> roundTripTime() const noexcept;

    /// Reliable messages queued or sent but not acknowledged yet.
    [[nodiscard]]
    std::size_t pendingReliableCount() const noexcept;

private:
    struct SentPacket {
        std::uint16_t sequence{};
        Clock::time_point time;
        std::optional<std::uint16_t> reliableId;
        bool valid{false};
        bool acknowledged{false};
    };

    struct PendingReliable {
        std::uint16_t reliableId{};
        protocol::Payload payload;
        std::optional<Clock::time_point> firstSent;
        Clock::time_point lastSent;
    };

    // Sent packets are remembered in a ring indexed by sequence: memory stays
    // bounded, and an acknowledgement older than the ring is simply ignored.
    static constexpr std::size_t SentHistorySize = 1024;

    [[nodiscard]]
    protocol::Packet nextPacket(protocol::Payload payload, std::uint16_t reliableId);
    void recordSent(const protocol::Packet& packet, Clock::time_point now,
        std::optional<std::uint16_t> reliableId);
    void acknowledge(std::uint16_t sequence, Clock::time_point now);
    [[nodiscard]]
    bool acceptSequence(std::uint16_t sequence) noexcept;
    [[nodiscard]]
    Clock::duration resendDelay() const noexcept;

    std::uint32_t _sessionToken;

    // Sending side
    std::uint16_t _nextSequence{0};
    std::uint16_t _nextReliableId{0};
    std::array<SentPacket, SentHistorySize> _sent{};
    std::deque<PendingReliable> _pending; // In reliableId order
    std::optional<Clock::duration> _roundTripTime;

    // Receiving side
    std::optional<std::uint16_t> _newestReceived;
    std::uint32_t _receivedBits{0}; // Bit i <=> _newestReceived - 1 - i was received
    std::uint16_t _nextDeliveredReliableId{0};
    std::map<std::uint16_t, protocol::Payload> _earlyReliable;
};

} // namespace network
