#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

/// Encryption of a session between one client and the server (issue #97).
///
///     // Each side, during the connection:
///     KeyPair ours = generateKeyPair();                 // Send ours.publicKey to the peer
///     auto key = deriveSessionKey(ours, theirs, Role::Client); // Same key on both sides
///     SessionCipher cipher(*key, Role::Client);
///
///     // Each packet:
///     auto size = cipher.seal(header, payload, output);  // Sender
///     auto size = cipher.open(header, sealed, output);   // Receiver
///
/// A sealed payload is: counter (8 bytes) | encrypted payload | tag (16 bytes).
/// See README.md for the reasons behind each choice.
namespace network::crypto {

inline constexpr std::size_t KeySize = 32;
inline constexpr std::size_t CounterSize = 8;
inline constexpr std::size_t TagSize = 16;
/// Bytes that sealing adds to a payload.
inline constexpr std::size_t Overhead = CounterSize + TagSize;

using PublicKey = std::array<std::byte, KeySize>; // Can travel in clear
using SecretKey = std::array<std::byte, KeySize>; // Never leaves this machine
using SessionKey = std::array<std::byte, KeySize>; // Shared by the two sides only

/// Which side of the session we are. It keeps the two directions apart: a
/// packet cannot be sent back to the side that wrote it.
enum class Role : std::uint8_t { Client = 0, Server = 1 };

struct KeyPair {
    SecretKey secret;
    PublicKey publicKey;
};

/// Creates a new key pair from the operating system's secure random source.
[[nodiscard]]
KeyPair generateKeyPair();

/// Combines our secret with the peer's public key: both sides get the same
/// session key. Returns nothing if the peer's key is invalid (it would give a
/// session key that anybody can compute).
[[nodiscard]]
std::optional<SessionKey> deriveSessionKey(
    const KeyPair& ours, const PublicKey& theirs, Role ourRole);

/// Remembers which counters were already received, to refuse replayed packets.
/// Like `ackBits`, it keeps the newest counter and one bit per older counter.
class ReplayWindow {
public:
    /// How far behind the newest counter a packet is still accepted.
    static constexpr std::uint64_t Size = 64;

    /// True if `counter` was never seen and is not too old.
    [[nodiscard]]
    bool isFresh(std::uint64_t counter) const;

    void markSeen(std::uint64_t counter);

private:
    bool _empty{true};
    std::uint64_t _newest{0};
    std::uint64_t _seen{0}; // Bit i set: counter (_newest - i) was received
};

enum class SealError : std::uint8_t {
    TooSmall, // Not a datagram: shorter than its header
    BufferTooSmall, // The output cannot hold the payload + Overhead
};

enum class OpenError : std::uint8_t {
    TooSmall, // Shorter than Overhead: it cannot be a sealed payload
    BufferTooSmall, // The output cannot hold the payload
    Replayed, // Counter already received, or too old
    Forged, // Wrong tag: modified, forged, or sealed with another key
};

/// Encrypts what one side sends and decrypts what it receives.
class SessionCipher {
public:
    SessionCipher(const SessionKey& key, Role ourRole);

    /// Writes the sealed `payload` into `output` and returns its size.
    /// `header` stays readable but is protected: changing it makes open() fail.
    /// `output` must not overlap `payload`.
    [[nodiscard]]
    std::expected<std::size_t, SealError> seal(std::span<const std::byte> header,
        std::span<const std::byte> payload, std::span<std::byte> output);

    /// Checks `sealed`, writes the original payload into `output` and returns
    /// its size. On failure the packet must be dropped: the state is unchanged.
    /// `output` must not overlap `sealed`.
    [[nodiscard]]
    std::expected<std::size_t, OpenError> open(std::span<const std::byte> header,
        std::span<const std::byte> sealed, std::span<std::byte> output);

private:
    SessionKey _key;
    Role _ourRole;
    std::uint64_t _nextCounter{0}; // Never goes back: a counter is used only once
    ReplayWindow _received;
};

} // namespace network::crypto
