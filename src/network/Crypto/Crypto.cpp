#include "network/Crypto/Crypto.hpp"

#include <algorithm>
#include <monocypher.h>
#include <random>

namespace network::crypto {

namespace {

constexpr std::size_t NonceSize = 24;

// Monocypher takes bytes as uint8_t, the network code uses std::byte
const std::uint8_t* rawIn(std::span<const std::byte> bytes)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): same bytes, other type
    return reinterpret_cast<const std::uint8_t*>(bytes.data());
}

std::uint8_t* rawOut(std::span<std::byte> bytes)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): same bytes, other type
    return reinterpret_cast<std::uint8_t*>(bytes.data());
}

void writeCounter(std::uint64_t counter, std::span<std::byte> output)
{
    for (std::size_t i = 0; i < CounterSize; ++i) { // Big-endian, like the whole protocol
        output[i] = static_cast<std::byte>(counter >> (8U * (CounterSize - 1 - i)));
    }
}

std::uint64_t readCounter(std::span<const std::byte> input)
{
    std::uint64_t counter = 0;
    for (std::size_t i = 0; i < CounterSize; ++i) {
        counter = (counter << 8U) | std::to_integer<std::uint64_t>(input[i]);
    }
    return counter;
}

/// The number that makes each encryption unique: never twice with one key.
/// The key already belongs to one session; the role separates the directions,
/// since both sides count from 0; the counter separates the packets.
std::array<std::byte, NonceSize> makeNonce(Role sender, std::uint64_t counter)
{
    std::array<std::byte, NonceSize> nonce{}; // The unused end stays at zero
    nonce[0] = static_cast<std::byte>(sender);
    writeCounter(counter, std::span(nonce).subspan(1, CounterSize));
    return nonce;
}

Role otherRole(Role role)
{
    return role == Role::Client ? Role::Server : Role::Client;
}

} // namespace

KeyPair generateKeyPair()
{
    KeyPair keys{};
    std::random_device random; // Same secure source as the session tokens
    for (std::byte& byte : keys.secret) {
        byte = static_cast<std::byte>(random() & 0xFFU);
    }
    crypto_x25519_public_key(rawOut(keys.publicKey), rawIn(keys.secret));
    return keys;
}

std::optional<SessionKey> deriveSessionKey(
    const KeyPair& ours, const PublicKey& theirs, Role ourRole)
{
    std::array<std::byte, KeySize> shared{};
    crypto_x25519(rawOut(shared), rawIn(ours.secret), rawIn(theirs));

    // An invalid public key gives an all-zero result, known to anybody
    const std::array<std::byte, KeySize> zero{};
    if (crypto_verify32(rawIn(shared), rawIn(zero)) == 0) {
        return std::nullopt;
    }

    // The raw result is not used as a key: it is hashed with both public keys,
    // always in the same order, so that the key depends on this exact exchange
    const PublicKey& clientKey = ourRole == Role::Client ? ours.publicKey : theirs;
    const PublicKey& serverKey = ourRole == Role::Client ? theirs : ours.publicKey;
    std::array<std::byte, 3 * KeySize> material{};
    std::ranges::copy(shared, material.begin());
    std::ranges::copy(clientKey, material.begin() + KeySize);
    std::ranges::copy(serverKey, material.begin() + (2 * KeySize));

    SessionKey key{};
    crypto_blake2b(rawOut(key), key.size(), rawIn(material), material.size());

    crypto_wipe(rawOut(shared), shared.size());
    crypto_wipe(rawOut(material), material.size());
    return key;
}

bool ReplayWindow::isFresh(std::uint64_t counter) const
{
    if (_empty || counter > _newest) {
        return true;
    }
    const std::uint64_t age = _newest - counter;
    return age < Size && ((_seen >> age) & 1U) == 0;
}

void ReplayWindow::markSeen(std::uint64_t counter)
{
    if (_empty) {
        _empty = false;
        _newest = counter;
        _seen = 1;
    } else if (counter > _newest) {
        const std::uint64_t shift = counter - _newest;
        _seen = shift < Size ? (_seen << shift) | 1U : 1U; // Shifting by 64 bits is undefined
        _newest = counter;
    } else {
        _seen |= std::uint64_t{1} << (_newest - counter);
    }
}

SessionCipher::SessionCipher(const SessionKey& key, Role ourRole)
    : _key(key)
    , _ourRole(ourRole)
{
}

std::expected<std::size_t, SealError> SessionCipher::seal(std::span<const std::byte> header,
    std::span<const std::byte> payload, std::span<std::byte> output)
{
    const std::size_t sealedSize = payload.size() + Overhead;
    if (output.size() < sealedSize) {
        return std::unexpected(SealError::BufferTooSmall);
    }

    const std::uint64_t counter = _nextCounter++;
    const auto nonce = makeNonce(_ourRole, counter);
    writeCounter(counter, output);
    const std::span<std::byte> encrypted = output.subspan(CounterSize, payload.size());
    const std::span<std::byte> tag = output.subspan(CounterSize + payload.size(), TagSize);

    crypto_aead_lock(rawOut(encrypted), rawOut(tag), rawIn(_key), rawIn(nonce), rawIn(header),
        header.size(), rawIn(payload), payload.size());
    return sealedSize;
}

std::expected<std::size_t, OpenError> SessionCipher::open(std::span<const std::byte> header,
    std::span<const std::byte> sealed, std::span<std::byte> output)
{
    if (sealed.size() < Overhead) {
        return std::unexpected(OpenError::TooSmall);
    }
    const std::size_t payloadSize = sealed.size() - Overhead;
    if (output.size() < payloadSize) {
        return std::unexpected(OpenError::BufferTooSmall);
    }

    const std::uint64_t counter = readCounter(sealed);
    if (!_received.isFresh(counter)) {
        return std::unexpected(OpenError::Replayed);
    }

    const auto nonce = makeNonce(otherRole(_ourRole), counter);
    const std::span<const std::byte> encrypted = sealed.subspan(CounterSize, payloadSize);
    const std::span<const std::byte> tag = sealed.subspan(CounterSize + payloadSize, TagSize);

    const int result = crypto_aead_unlock(rawOut(output), rawIn(tag), rawIn(_key), rawIn(nonce),
        rawIn(header), header.size(), rawIn(encrypted), payloadSize);
    if (result != 0) {
        return std::unexpected(OpenError::Forged);
    }

    // Only now: until the tag is checked, the counter may be an attacker's
    _received.markSeen(counter);
    return payloadSize;
}

} // namespace network::crypto
