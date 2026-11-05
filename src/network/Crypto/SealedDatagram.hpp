#pragma once

#include "network/Crypto/Crypto.hpp"

#include <cstddef>
#include <expected>
#include <span>

/// Applies a `SessionCipher` to a whole datagram (docs/protocol.md §3.3).
///
///     auto size = encode(packet, buffer);                     // Sender
///     auto sealed = sealDatagram(cipher, plain, output);
///
///     auto opened = openDatagram(cipher, datagram, output);   // Receiver
///     auto packet = decode(plain, direction);
///
/// The header stays readable, since the receiver needs its token to find the
/// session, but it is protected: changing any of its bytes makes opening fail.
/// `output` must not overlap the input.
namespace network::crypto {

/// Seals a datagram written by `encode()`: same header, except `payloadSize`
/// which grows by `Overhead`. Returns the size of the sealed datagram.
[[nodiscard]]
std::expected<std::size_t, SealError> sealDatagram(
    SessionCipher& cipher, std::span<const std::byte> datagram, std::span<std::byte> output);

/// The opposite of `sealDatagram()`: writes the datagram `decode()` expects and
/// returns its size. On failure the datagram must be dropped.
[[nodiscard]]
std::expected<std::size_t, OpenError> openDatagram(
    SessionCipher& cipher, std::span<const std::byte> datagram, std::span<std::byte> output);

} // namespace network::crypto
