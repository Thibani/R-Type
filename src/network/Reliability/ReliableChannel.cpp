#include "network/Reliability/ReliableChannel.hpp"

#include <algorithm>
#include <type_traits>
#include <utility>
#include <variant>

namespace network {

using namespace protocol;

namespace {

bool isReliable(const Payload& payload)
{
    return std::visit(
        [](const auto& value) { return std::decay_t<decltype(value)>::Reliable; }, payload);
}

} // namespace

ReliableChannel::ReliableChannel(std::uint32_t sessionToken) noexcept
    : _sessionToken(sessionToken)
{
}

void ReliableChannel::setSessionToken(std::uint32_t sessionToken) noexcept
{
    _sessionToken = sessionToken;
}

// --- Sending ------------------------------------------------------------------

Packet ReliableChannel::wrap(Payload payload, Clock::time_point now)
{
    Packet packet = nextPacket(std::move(payload), 0);
    recordSent(packet, now, std::nullopt);
    return packet;
}

void ReliableChannel::queueReliable(Payload payload)
{
    _pending.push_back(PendingReliable{
        .reliableId = _nextReliableId++,
        .payload = std::move(payload),
        .firstSent = std::nullopt,
        .lastSent = {},
    });
}

std::vector<Packet> ReliableChannel::takeDue(Clock::time_point now)
{
    std::vector<Packet> due;
    if (_pending.empty()) {
        return due;
    }
    const std::uint16_t oldest = _pending.front().reliableId;
    for (PendingReliable& message : _pending) {
        // Sending window: the peer buffers at most 64 messages ahead of the one
        // it waits for. Sending further would get a healthy peer disconnected.
        if (static_cast<std::uint16_t>(message.reliableId - oldest) > MaxBufferedReliable) {
            break; // _pending is in reliableId order: the rest is further still
        }
        if (message.firstSent && now - message.lastSent < resendDelay()) {
            continue;
        }
        // A resend is a new packet (new sequence) carrying the same reliableId
        Packet packet = nextPacket(message.payload, message.reliableId);
        recordSent(packet, now, message.reliableId);
        if (!message.firstSent) {
            message.firstSent = now;
        }
        message.lastSent = now;
        due.push_back(std::move(packet));
    }
    return due;
}

bool ReliableChannel::timedOut(Clock::time_point now) const noexcept
{
    return std::ranges::any_of(_pending, [&](const PendingReliable& message) {
        return message.firstSent && now - *message.firstSent >= ReliableTimeout;
    });
}

Packet ReliableChannel::nextPacket(Payload payload, std::uint16_t reliableId)
{
    std::optional<Ack> ack;
    if (_newestReceived) {
        ack = Ack{.sequence = *_newestReceived, .bits = _receivedBits};
    }
    return Packet{
        .sessionToken = _sessionToken,
        .sequence = _nextSequence++,
        .ack = ack,
        .reliableId = reliableId,
        .payload = std::move(payload),
    };
}

void ReliableChannel::recordSent(
    const Packet& packet, Clock::time_point now, std::optional<std::uint16_t> reliableId)
{
    _sent[packet.sequence % SentHistorySize] = SentPacket{
        .sequence = packet.sequence,
        .time = now,
        .reliableId = reliableId,
        .valid = true,
        .acknowledged = false,
    };
}

ReliableChannel::Clock::duration ReliableChannel::resendDelay() const noexcept
{
    const Clock::duration minimum = MinResendDelay;
    return _roundTripTime ? std::max(minimum, 2 * *_roundTripTime) : minimum;
}

// --- Receiving ----------------------------------------------------------------

std::expected<std::vector<Payload>, ChannelError> ReliableChannel::receive(
    const Packet& packet, Clock::time_point now)
{
    if (!acceptSequence(packet.sequence)) {
        return std::vector<Payload>{}; // Duplicate or too old: already handled
    }

    if (packet.ack) {
        acknowledge(packet.ack->sequence, now);
        for (std::uint16_t i = 0; i < AckWindow; ++i) {
            if ((packet.ack->bits & (1U << i)) != 0) {
                acknowledge(static_cast<std::uint16_t>(packet.ack->sequence - 1 - i), now);
            }
        }
    }

    std::vector<Payload> delivered;
    if (!isReliable(packet.payload)) {
        delivered.push_back(packet.payload);
        return delivered;
    }

    const std::uint16_t id = packet.reliableId;
    if (id == _nextDeliveredReliableId) {
        delivered.push_back(packet.payload);
        ++_nextDeliveredReliableId;
        // It may unblock the messages that arrived before it
        for (auto next = _earlyReliable.find(_nextDeliveredReliableId);
            next != _earlyReliable.end(); next = _earlyReliable.find(_nextDeliveredReliableId)) {
            delivered.push_back(std::move(next->second));
            _earlyReliable.erase(next);
            ++_nextDeliveredReliableId;
        }
    } else if (isNewer(id, _nextDeliveredReliableId) && !_earlyReliable.contains(id)) {
        if (_earlyReliable.size() >= MaxBufferedReliable) {
            return std::unexpected(ChannelError::ReliableBufferFull);
        }
        _earlyReliable.emplace(id, packet.payload);
    }
    // Otherwise: already delivered (its acknowledgement was lost), dropped
    return delivered;
}

bool ReliableChannel::acceptSequence(std::uint16_t sequence) noexcept
{
    if (!_newestReceived) {
        _newestReceived = sequence;
        _receivedBits = 0;
        return true;
    }
    if (sequence == *_newestReceived) {
        return false;
    }

    if (isNewer(sequence, *_newestReceived)) {
        const auto shift = static_cast<std::uint16_t>(sequence - *_newestReceived);
        _receivedBits = shift < 32 ? _receivedBits << shift : 0U;
        if (shift <= AckWindow) {
            _receivedBits |= 1U << (shift - 1U); // The previous newest
        }
        _newestReceived = sequence;
        return true;
    }

    const auto age = static_cast<std::uint16_t>(*_newestReceived - sequence);
    if (age > AckWindow) {
        return false; // Too old to tell whether it is a duplicate (§6.1)
    }
    const std::uint32_t bit = 1U << (age - 1U);
    if ((_receivedBits & bit) != 0) {
        return false;
    }
    _receivedBits |= bit;
    return true;
}

void ReliableChannel::acknowledge(std::uint16_t sequence, Clock::time_point now)
{
    SentPacket& sent = _sent[sequence % SentHistorySize];
    if (!sent.valid || sent.sequence != sequence || sent.acknowledged) {
        return; // Never sent, overwritten by a newer packet, or already counted
    }
    sent.acknowledged = true;

    // Smoothed like TCP (RFC 6298): one eighth of each new sample
    const Clock::duration sample = now - sent.time;
    _roundTripTime = _roundTripTime ? *_roundTripTime + (sample - *_roundTripTime) / 8 : sample;

    if (sent.reliableId) {
        std::erase_if(_pending,
            [&](const PendingReliable& message) { return message.reliableId == *sent.reliableId; });
    }
}

// --- Statistics ---------------------------------------------------------------

std::optional<ReliableChannel::Clock::duration> ReliableChannel::roundTripTime() const noexcept
{
    return _roundTripTime;
}

std::size_t ReliableChannel::pendingReliableCount() const noexcept
{
    return _pending.size();
}

} // namespace network
