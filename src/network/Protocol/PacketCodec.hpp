#pragma once

#include "network/Protocol/Packets.hpp"
#include "network/Protocol/Protocol.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

/// Turns `Packet`s into datagrams and back (docs/protocol.md).
///
///     DatagramBuffer buffer{};
///     auto size = encode(packet, buffer);              // packet -> bytes
///     auto decoded = decode(received, Direction::ClientToServer); // bytes -> packet
///
/// Decoding applies every validation rule of §7 that does not need session
/// state. A datagram that fails is reported with a `DecodeError` and must be
/// dropped: nothing in it can be trusted. Session checks (rule 10: the token
/// matches the sender's address) belong to the connection layer.
namespace network::protocol {

/// Why a datagram was rejected, in the order the checks are made.
enum class DecodeError : std::uint8_t {
    TooSmall, // Shorter than the header
    TooLarge, // Longer than MaxDatagramSize
    BadMagic, // Not an R-Type packet
    ReservedBitsSet, // Reserved header byte or flag bit is not 0
    PayloadSizeMismatch, // payloadSize does not match the datagram size
    UnsupportedVersion, // Other protocol version
    UnknownType, // Unassigned packet type
    WrongDirection, // E.g. a SNAPSHOT received by the server
    ReliableFlagMismatch, // RELIABLE flag inconsistent with the packet type
    BadPayloadSize, // Payload too short, too long, or count out of range
    InvalidValue, // Enumeration or id out of range
    InvalidName, // Name is not valid zero-padded UTF-8
};

enum class EncodeError : std::uint8_t {
    InvalidPayload, // The payload breaks a rule the receiver would reject
    BufferTooSmall, // The output buffer cannot hold the datagram
};

/// Encodes `packet` into `output` and returns the number of bytes written.
/// The header's type, payloadSize and RELIABLE flag are derived from the payload.
/// Fails rather than producing a datagram that `decode()` would reject.
[[nodiscard]]
std::expected<std::size_t, EncodeError> encode(const Packet& packet, std::span<std::byte> output);

/// Reads and checks only the header (§7 rules 1, 2, 5 and 6).
///
/// Useful to answer a CONNECT_REQUEST of another protocol version with
/// CONNECT_REJECT (VERSION_MISMATCH): `decode()` rejects it before that.
[[nodiscard]]
std::expected<PacketHeader, DecodeError> decodeHeader(std::span<const std::byte> datagram) noexcept;

/// Decodes and fully validates a datagram travelling in `direction`
/// (`ClientToServer` on the server, `ServerToClient` on the client).
///
/// Never reads past `datagram` and never allocates more than the datagram
/// size: counts are checked against the received bytes before any allocation.
[[nodiscard]]
std::expected<Packet, DecodeError> decode(std::span<const std::byte> datagram, Direction direction);

/// True if `name` can be sent: at most 16 bytes, valid UTF-8, no '\0'.
[[nodiscard]]
bool isValidName(std::string_view name) noexcept;

/// Human readable name of an error, for logs.
[[nodiscard]]
std::string_view toString(DecodeError error) noexcept;

[[nodiscard]]
std::string_view toString(EncodeError error) noexcept;

} // namespace network::protocol
