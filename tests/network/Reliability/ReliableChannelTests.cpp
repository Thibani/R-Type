// Two channels talk to each other through vectors of packets: the tests decide
// which packets are lost, duplicated or reordered, and what time it is.

#include "network/Reliability/ReliableChannel.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <numeric>
#include <random>
#include <utility>
#include <variant>
#include <vector>

using network::ChannelError;
using network::isNewer;
using network::ReliableChannel;
using namespace network::protocol;
using namespace std::chrono_literals;

namespace {

using TimePoint = ReliableChannel::Clock::time_point;

constexpr TimePoint Start{};

/// A reliable message numbered `n`, easy to recognise once delivered.
Payload message(std::uint8_t n)
{
    return PlayerDied{.playerId = n, .killerNetId = NoEntity};
}

/// A packet as if sent by a peer, for tests that only look at the receiver.
Packet fromPeer(std::uint16_t sequence, Payload payload = Heartbeat{}, std::uint16_t reliableId = 0)
{
    return Packet{
        .sessionToken = 1,
        .sequence = sequence,
        .ack = std::nullopt,
        .reliableId = reliableId,
        .payload = std::move(payload),
    };
}

std::vector<Payload> receiveOrFail(ReliableChannel& channel, const Packet& packet, TimePoint now)
{
    const auto delivered = channel.receive(packet, now);
    EXPECT_TRUE(delivered.has_value());
    return delivered.value_or(std::vector<Payload>{});
}

std::vector<std::uint8_t> playerIdsOf(const std::vector<Payload>& payloads)
{
    std::vector<std::uint8_t> ids;
    ids.reserve(payloads.size());
    for (const Payload& payload : payloads) {
        ids.push_back(std::get<PlayerDied>(payload).playerId);
    }
    return ids;
}

/// Loses, duplicates and reorders packets, reproducibly.
class BadNetwork {
public:
    std::vector<Packet> transmit(std::vector<Packet> packets)
    {
        std::vector<Packet> arriving;
        for (Packet& packet : packets) {
            if (_lost(_random)) {
                continue;
            }
            if (_duplicated(_random)) {
                arriving.push_back(packet);
            }
            arriving.push_back(std::move(packet));
        }
        std::ranges::shuffle(arriving, _random);
        return arriving;
    }

private:
    std::mt19937 _random{2024}; // NOLINT(bugprone-random-generator-seed): reproducible failures
    std::bernoulli_distribution _lost{0.4};
    std::bernoulli_distribution _duplicated{0.1};
};

} // namespace

// --- Sequence numbers (§6.1) --------------------------------------------------

TEST(ReliableChannel, NewerHandlesWrapAround)
{
    static_assert(isNewer(1, 0));
    static_assert(!isNewer(0, 1));
    static_assert(!isNewer(5, 5));
    static_assert(isNewer(0, 65535)); // 0 comes right after 65535
    static_assert(!isNewer(65535, 0));
    static_assert(isNewer(32768, 0)); // Exactly half way counts as newer
    static_assert(!isNewer(32769, 0));
}

TEST(ReliableChannel, EverySentPacketGetsTheNextSequence)
{
    ReliableChannel channel(42);

    const Packet first = channel.wrap(Heartbeat{}, Start);
    const Packet second = channel.wrap(Heartbeat{}, Start);

    EXPECT_EQ(first.sequence, 0);
    EXPECT_EQ(second.sequence, 1);
    EXPECT_EQ(first.sessionToken, 42U);
    EXPECT_FALSE(first.ack.has_value()); // Nothing received yet: HAS_ACK clear
}

TEST(ReliableChannel, SequenceWrapsAfter65535)
{
    ReliableChannel channel;
    for (int i = 0; i < 65535; ++i) {
        (void)channel.wrap(Heartbeat{}, Start);
    }

    EXPECT_EQ(channel.wrap(Heartbeat{}, Start).sequence, 65535);
    EXPECT_EQ(channel.wrap(Heartbeat{}, Start).sequence, 0);
}

TEST(ReliableChannel, AckDescribesWhatWasReceived)
{
    ReliableChannel channel;
    for (const std::uint16_t sequence : std::initializer_list<std::uint16_t>{0, 1, 3}) { // 2 lost
        (void)receiveOrFail(channel, fromPeer(sequence), Start);
    }

    const Packet reply = channel.wrap(Heartbeat{}, Start);

    ASSERT_TRUE(reply.ack.has_value());
    EXPECT_EQ(reply.ack->sequence, 3);
    EXPECT_EQ(reply.ack->bits, 0b110U); // bit 0: 2 missing, bit 1: 1, bit 2: 0
}

TEST(ReliableChannel, AckFollowsTheWrapAround)
{
    ReliableChannel channel;
    (void)receiveOrFail(channel, fromPeer(65535), Start);
    (void)receiveOrFail(channel, fromPeer(0), Start);

    const Packet reply = channel.wrap(Heartbeat{}, Start);

    ASSERT_TRUE(reply.ack.has_value());
    EXPECT_EQ(reply.ack->sequence, 0);
    EXPECT_EQ(reply.ack->bits, 0b1U); // 65535 received too
}

TEST(ReliableChannel, DuplicatePacketIsDropped)
{
    ReliableChannel channel;

    EXPECT_EQ(receiveOrFail(channel, fromPeer(7), Start).size(), 1U);
    EXPECT_TRUE(receiveOrFail(channel, fromPeer(7), Start).empty());
}

TEST(ReliableChannel, LatePacketIsDeliveredOnce)
{
    ReliableChannel channel;
    (void)receiveOrFail(channel, fromPeer(10), Start);

    EXPECT_EQ(receiveOrFail(channel, fromPeer(9), Start).size(), 1U); // Reordered, not lost
    EXPECT_TRUE(receiveOrFail(channel, fromPeer(9), Start).empty());
}

TEST(ReliableChannel, PacketOlderThanTheWindowIsDropped)
{
    ReliableChannel channel;
    (void)receiveOrFail(channel, fromPeer(100), Start);

    EXPECT_EQ(receiveOrFail(channel, fromPeer(68), Start).size(), 1U); // 32 old: still known
    EXPECT_TRUE(receiveOrFail(channel, fromPeer(67), Start).empty()); // 33 old: unknown
}

// --- Reliable messages (§6.2) -------------------------------------------------

TEST(ReliableChannel, ReliableMessageIsDeliveredAndAcknowledged)
{
    ReliableChannel server;
    ReliableChannel client;
    server.queueReliable(message(1));

    const auto sent = server.takeDue(Start);
    ASSERT_EQ(sent.size(), 1U);
    EXPECT_EQ(
        playerIdsOf(receiveOrFail(client, sent[0], Start + 10ms)), std::vector<std::uint8_t>{1});

    // Any packet from the client carries the acknowledgement back
    (void)receiveOrFail(server, client.wrap(Heartbeat{}, Start + 10ms), Start + 20ms);
    EXPECT_EQ(server.pendingReliableCount(), 0U);
    EXPECT_TRUE(server.takeDue(Start + 1s).empty());
}

TEST(ReliableChannel, LostReliableMessageIsResentWithTheSameId)
{
    ReliableChannel server;
    server.queueReliable(message(1));

    const auto first = server.takeDue(Start); // Lost
    EXPECT_TRUE(server.takeDue(Start + 99ms).empty()); // Not due yet
    const auto resent = server.takeDue(Start + 100ms);

    ASSERT_EQ(first.size(), 1U);
    ASSERT_EQ(resent.size(), 1U);
    EXPECT_EQ(resent[0].reliableId, first[0].reliableId);
    EXPECT_NE(resent[0].sequence, first[0].sequence); // A resend is a new packet
    EXPECT_EQ(resent[0].payload, first[0].payload);
}

TEST(ReliableChannel, ReliableMessageIsDeliveredOnlyOnce)
{
    ReliableChannel server;
    ReliableChannel client;
    server.queueReliable(message(1));
    const auto original = server.takeDue(Start);
    const auto resent = server.takeDue(Start + 100ms); // The acknowledgement was lost

    EXPECT_EQ(receiveOrFail(client, original[0], Start + 10ms).size(), 1U);
    EXPECT_TRUE(receiveOrFail(client, resent[0], Start + 110ms).empty());
}

TEST(ReliableChannel, ReliableMessagesAreDeliveredInOrder)
{
    ReliableChannel server;
    ReliableChannel client;
    for (std::uint8_t n = 0; n < 3; ++n) {
        server.queueReliable(message(n));
    }
    const auto sent = server.takeDue(Start);
    ASSERT_EQ(sent.size(), 3U);

    // 2 and 1 arrive before 0: they wait for it
    EXPECT_TRUE(receiveOrFail(client, sent[2], Start).empty());
    EXPECT_TRUE(receiveOrFail(client, sent[1], Start).empty());
    EXPECT_EQ(
        playerIdsOf(receiveOrFail(client, sent[0], Start)), (std::vector<std::uint8_t>{0, 1, 2}));
}

TEST(ReliableChannel, TooManyEarlyReliableMessagesIsAnError)
{
    // Only a misbehaving peer can do this: an honest sender never goes more
    // than 64 messages ahead (see SenderNeverGoesFurtherThanTheWindow).
    ReliableChannel client;
    constexpr auto limit = static_cast<std::uint16_t>(ReliableChannel::MaxBufferedReliable);

    // Message 0 never arrives: 1..64 fill the buffer, 65 overflows it
    for (std::uint16_t id = 1; id <= limit; ++id) {
        EXPECT_TRUE(receiveOrFail(client, fromPeer(id, message(0), id), Start).empty());
    }
    const auto overflow = client.receive(fromPeer(limit + 1, message(0), limit + 1), Start);
    ASSERT_FALSE(overflow.has_value());
    EXPECT_EQ(overflow.error(), ChannelError::ReliableBufferFull);
}

TEST(ReliableChannel, SenderNeverGoesFurtherThanTheWindow)
{
    ReliableChannel server;
    ReliableChannel client;
    for (std::uint8_t n = 0; n < 100; ++n) {
        server.queueReliable(message(n));
    }

    // 0 (waited for) + 64 that the peer can buffer; 65..99 wait
    const auto sent = server.takeDue(Start);
    ASSERT_EQ(sent.size(), ReliableChannel::MaxBufferedReliable + 1);
    EXPECT_EQ(sent.back().reliableId, ReliableChannel::MaxBufferedReliable);

    // Once 0 is acknowledged, the window moves by one
    (void)receiveOrFail(client, sent[0], Start);
    (void)receiveOrFail(server, client.wrap(Heartbeat{}, Start), Start + 10ms);
    const auto next = server.takeDue(Start + 20ms);
    ASSERT_EQ(next.size(), 1U);
    EXPECT_EQ(next[0].reliableId, ReliableChannel::MaxBufferedReliable + 1);
}

TEST(ReliableChannel, UnacknowledgedForFiveSecondsMeansTimedOut)
{
    ReliableChannel server;
    server.queueReliable(message(1));
    EXPECT_FALSE(server.timedOut(Start + 1h)); // Queued but never sent: no clock yet

    (void)server.takeDue(Start);

    EXPECT_FALSE(server.timedOut(Start + 4999ms));
    EXPECT_TRUE(server.timedOut(Start + 5s));
}

TEST(ReliableChannel, RoundTripTimeStretchesTheResendDelay)
{
    ReliableChannel server;
    ReliableChannel client;
    EXPECT_FALSE(server.roundTripTime().has_value());

    // One exchange taking 80 ms: resend delay becomes max(100 ms, 160 ms)
    const Packet ping = server.wrap(Heartbeat{}, Start);
    (void)receiveOrFail(client, ping, Start + 40ms);
    (void)receiveOrFail(server, client.wrap(Heartbeat{}, Start + 40ms), Start + 80ms);
    ASSERT_TRUE(server.roundTripTime().has_value());
    EXPECT_EQ(*server.roundTripTime(), 80ms);

    server.queueReliable(message(1));
    (void)server.takeDue(Start + 100ms);
    EXPECT_TRUE(server.takeDue(Start + 259ms).empty());
    EXPECT_EQ(server.takeDue(Start + 260ms).size(), 1U);
}

TEST(ReliableChannel, EveryMessageArrivesInOrderOverABadNetwork)
{
    // 40% loss in both directions, 10% duplicates, random order: all 200
    // messages must arrive exactly once, in order, without a timeout.
    BadNetwork network;
    ReliableChannel server;
    ReliableChannel client;
    constexpr std::uint8_t messageCount = 200;
    for (std::uint8_t n = 0; n < messageCount; ++n) {
        server.queueReliable(message(n));
    }

    std::vector<std::uint8_t> received;
    TimePoint now = Start;
    for (int tick = 0; tick < 1000 && server.pendingReliableCount() > 0; ++tick) {
        now += 16ms;
        for (const Packet& packet : network.transmit(server.takeDue(now))) {
            std::ranges::copy(
                playerIdsOf(receiveOrFail(client, packet, now)), std::back_inserter(received));
        }
        for (const Packet& packet : network.transmit({client.wrap(Heartbeat{}, now)})) {
            (void)receiveOrFail(server, packet, now);
        }
        ASSERT_FALSE(server.timedOut(now));
    }

    std::vector<std::uint8_t> expected(messageCount);
    std::ranges::iota(expected, std::uint8_t{0});
    EXPECT_EQ(received, expected);
}
