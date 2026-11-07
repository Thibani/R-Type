// Covers issue #97: a round trip in each direction, a tampered packet, a wrong
// key and a replayed packet, plus the attacks each protection exists for.

#include "network/Crypto/Crypto.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

using namespace network::crypto;

namespace {

using Bytes = std::vector<std::byte>;

Bytes bytes(std::string_view text)
{
    Bytes result;
    for (const char character : text) {
        result.push_back(static_cast<std::byte>(character));
    }
    return result;
}

const Bytes Header = bytes("packet header");
const Bytes Payload = bytes("player 2 fires");

/// Both ends of one session, created by a real key exchange.
struct Session {
    SessionCipher client;
    SessionCipher server;
};

Session makeSession()
{
    const KeyPair clientKeys = generateKeyPair();
    const KeyPair serverKeys = generateKeyPair();
    const auto clientKey = deriveSessionKey(clientKeys, serverKeys.publicKey, Role::Client);
    const auto serverKey = deriveSessionKey(serverKeys, clientKeys.publicKey, Role::Server);
    return {SessionCipher(clientKey.value(), Role::Client),
        SessionCipher(serverKey.value(), Role::Server)};
}

Bytes seal(SessionCipher& cipher, const Bytes& payload = Payload)
{
    Bytes output(payload.size() + Overhead);
    const auto size = cipher.seal(Header, payload, output);
    EXPECT_TRUE(size.has_value());
    return output;
}

std::expected<Bytes, OpenError> open(
    SessionCipher& cipher, const Bytes& sealed, const Bytes& header = Header)
{
    Bytes output(sealed.size());
    const auto size = cipher.open(header, sealed, output);
    if (!size) {
        return std::unexpected(size.error());
    }
    output.resize(*size);
    return output;
}

} // namespace

// --- Key exchange ---------------------------------------------------------------

TEST(KeyExchange, BothSidesGetTheSameKey)
{
    const KeyPair client = generateKeyPair();
    const KeyPair server = generateKeyPair();

    const auto clientKey = deriveSessionKey(client, server.publicKey, Role::Client);
    const auto serverKey = deriveSessionKey(server, client.publicKey, Role::Server);

    ASSERT_TRUE(clientKey.has_value());
    ASSERT_TRUE(serverKey.has_value());
    EXPECT_EQ(*clientKey, *serverKey);
}

TEST(KeyExchange, EachKeyPairIsDifferent)
{
    EXPECT_NE(generateKeyPair().secret, generateKeyPair().secret);
}

TEST(KeyExchange, EachSessionHasItsOwnKey)
{
    const KeyPair server = generateKeyPair();

    const auto first = deriveSessionKey(server, generateKeyPair().publicKey, Role::Server);
    const auto second = deriveSessionKey(server, generateKeyPair().publicKey, Role::Server);

    EXPECT_NE(first.value(), second.value());
}

TEST(KeyExchange, AnotherPlayersKeyDoesNotGiveTheSessionKey)
{
    const KeyPair client = generateKeyPair();
    const KeyPair server = generateKeyPair();
    const KeyPair eavesdropper = generateKeyPair();

    const auto real = deriveSessionKey(client, server.publicKey, Role::Client);
    const auto guess = deriveSessionKey(eavesdropper, server.publicKey, Role::Client);

    EXPECT_NE(real.value(), guess.value());
}

TEST(KeyExchange, AnAllZeroPublicKeyIsRefused)
{
    EXPECT_FALSE(deriveSessionKey(generateKeyPair(), PublicKey{}, Role::Server).has_value());
}

// --- Round trip -------------------------------------------------------------------

TEST(SessionCipher, ClientToServerRoundTrip)
{
    Session session = makeSession();

    EXPECT_EQ(open(session.server, seal(session.client)).value(), Payload);
}

TEST(SessionCipher, ServerToClientRoundTrip)
{
    Session session = makeSession();

    EXPECT_EQ(open(session.client, seal(session.server)).value(), Payload);
}

TEST(SessionCipher, AnEmptyPayloadRoundTrips) // HEARTBEAT has no payload
{
    Session session = makeSession();

    const Bytes sealed = seal(session.client, {});

    EXPECT_EQ(sealed.size(), Overhead);
    EXPECT_TRUE(open(session.server, sealed).value().empty());
}

TEST(SessionCipher, AddsExactlyTheOverhead)
{
    Session session = makeSession();
    Bytes output(Payload.size() + Overhead);

    EXPECT_EQ(session.client.seal(Header, Payload, output).value(), Payload.size() + 24);
}

TEST(SessionCipher, ThePayloadCannotBeReadOnTheNetwork)
{
    Session session = makeSession();

    const Bytes sealed = seal(session.client);

    EXPECT_EQ(std::ranges::search(sealed, Payload).begin(), sealed.end());
}

TEST(SessionCipher, TheSamePayloadGivesDifferentBytesEachTime)
{
    Session session = makeSession();

    const Bytes first = seal(session.client);
    const Bytes second = seal(session.client);

    // Compared without the counter: the encrypted bytes change too
    EXPECT_FALSE(std::equal(
        first.begin() + CounterSize, first.end(), second.begin() + CounterSize, second.end()));
}

// --- Tampered and forged packets ---------------------------------------------------

TEST(SessionCipher, ChangingAnyByteOfASealedPacketIsDetected)
{
    Session session = makeSession();
    const Bytes sealed = seal(session.client);

    for (std::size_t i = 0; i < sealed.size(); ++i) {
        Bytes tampered = sealed;
        tampered[i] ^= std::byte{0x01};
        EXPECT_FALSE(open(session.server, tampered).has_value()) << "byte " << i;
    }
    EXPECT_TRUE(open(session.server, sealed).has_value()); // The real one still passes
}

TEST(SessionCipher, ChangingTheHeaderIsDetected)
{
    Session session = makeSession();
    const Bytes sealed = seal(session.client);

    const auto opened = open(session.server, sealed, bytes("another header"));

    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error(), OpenError::Forged);
}

TEST(SessionCipher, TheWrongKeyIsRefused)
{
    Session session = makeSession();
    Session other = makeSession();

    const auto opened = open(other.server, seal(session.client));

    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error(), OpenError::Forged);
}

TEST(SessionCipher, APacketSentBackToItsAuthorIsRefused)
{
    Session session = makeSession();

    const auto opened = open(session.client, seal(session.client));

    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error(), OpenError::Forged);
}

TEST(SessionCipher, TooShortAPacketIsRefused)
{
    Session session = makeSession();

    const auto opened = open(session.server, Bytes(Overhead - 1));

    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error(), OpenError::TooSmall);
}

TEST(SessionCipher, RandomBytesAreRefusedWithoutCrashing)
{
    Session session = makeSession();

    for (std::size_t size = 0; size < 200; ++size) {
        Bytes garbage(size, static_cast<std::byte>(size));
        EXPECT_FALSE(open(session.server, garbage).has_value());
    }
}

// --- Replayed packets ---------------------------------------------------------------

TEST(SessionCipher, AReplayedPacketIsRefused)
{
    Session session = makeSession();
    const Bytes sealed = seal(session.client);
    ASSERT_TRUE(open(session.server, sealed).has_value());

    const auto replayed = open(session.server, sealed);

    ASSERT_FALSE(replayed.has_value());
    EXPECT_EQ(replayed.error(), OpenError::Replayed);
}

TEST(SessionCipher, PacketsArrivingOutOfOrderAreAccepted) // UDP does not keep the order
{
    Session session = makeSession();
    const Bytes first = seal(session.client);
    const Bytes second = seal(session.client);

    EXPECT_TRUE(open(session.server, second).has_value());
    EXPECT_TRUE(open(session.server, first).has_value());
}

TEST(SessionCipher, APacketTooFarBehindIsRefused)
{
    Session session = makeSession();
    const Bytes old = seal(session.client);
    for (std::uint64_t i = 0; i < ReplayWindow::Size; ++i) {
        ASSERT_TRUE(open(session.server, seal(session.client)).has_value());
    }

    const auto opened = open(session.server, old);

    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error(), OpenError::Replayed);
}

TEST(SessionCipher, AForgedPacketCannotBlockRealOnes)
{
    Session session = makeSession();
    const Bytes real = seal(session.client);

    // An attacker announces a huge counter: if it were recorded before checking
    // the tag, every real packet would then look too old
    Bytes forged = real;
    forged[0] = std::byte{0x7F};
    ASSERT_FALSE(open(session.server, forged).has_value());

    EXPECT_TRUE(open(session.server, real).has_value());
}

TEST(SessionCipher, TheCounterKeepsGoingWhereThe16BitSequenceWouldWrap)
{
    Session session = makeSession();
    constexpr int SixteenBitWrap = 65'536;
    const Bytes first = seal(session.client);
    ASSERT_TRUE(open(session.server, first).has_value());
    const auto withoutCounter
        = [](const Bytes& sealed) { return Bytes(sealed.begin() + CounterSize, sealed.end()); };

    for (int i = 1; i <= SixteenBitWrap; ++i) {
        const Bytes sealed = seal(session.client);
        ASSERT_TRUE(open(session.server, sealed).has_value()) << "packet " << i;
        // Same payload: identical bytes would mean that the nonce was used again
        ASSERT_NE(withoutCounter(sealed), withoutCounter(first)) << "packet " << i;
    }
    EXPECT_FALSE(open(session.server, first).has_value()); // Still refused after the wrap
}

// --- Replay window -------------------------------------------------------------------

TEST(ReplayWindow, EdgesOfTheWindow)
{
    ReplayWindow window;
    EXPECT_TRUE(window.isFresh(0));

    window.markSeen(100);
    EXPECT_FALSE(window.isFresh(100));
    EXPECT_TRUE(window.isFresh(101));
    EXPECT_TRUE(window.isFresh(100 - ReplayWindow::Size + 1)); // Oldest accepted
    EXPECT_FALSE(window.isFresh(100 - ReplayWindow::Size)); // First refused

    window.markSeen(99);
    EXPECT_FALSE(window.isFresh(99));
}

TEST(ReplayWindow, AJumpLargerThanTheWindowForgetsEverything)
{
    ReplayWindow window;
    window.markSeen(1);

    window.markSeen(1 + (3 * ReplayWindow::Size));

    EXPECT_FALSE(window.isFresh(1 + (3 * ReplayWindow::Size)));
    EXPECT_TRUE(window.isFresh(1 + (3 * ReplayWindow::Size) - 1));
    EXPECT_FALSE(window.isFresh(1));
}

// --- Buffers -------------------------------------------------------------------------

TEST(SessionCipher, SealRefusesAnOutputTooSmallAndKeepsItsCounter)
{
    Session session = makeSession();
    Bytes small(Payload.size() + Overhead - 1);

    const auto size = session.client.seal(Header, Payload, small);

    ASSERT_FALSE(size.has_value());
    EXPECT_EQ(size.error(), SealError::BufferTooSmall);
    EXPECT_EQ(open(session.server, seal(session.client)).value(), Payload);
}

TEST(SessionCipher, OpenRefusesAnOutputTooSmall)
{
    Session session = makeSession();
    const Bytes sealed = seal(session.client);
    Bytes small(Payload.size() - 1);

    const auto size = session.server.open(Header, sealed, small);

    ASSERT_FALSE(size.has_value());
    EXPECT_EQ(size.error(), OpenError::BufferTooSmall);
    EXPECT_TRUE(open(session.server, sealed).has_value()); // Not recorded as received
}

// --- Same bytes on every platform -------------------------------------------------------
// A Linux server and a Windows client must agree on every byte. The expected values
// come from other implementations: RFC 7748 for the keys, Python's hashlib and
// libsodium (PyNaCl) for the session key and the sealed payloads.

namespace {

template <std::size_t Size>
std::array<std::byte, Size> fromHex(std::string_view hex)
{
    std::array<std::byte, Size> result{};
    const auto digit
        = [](char character) { return character <= '9' ? character - '0' : character - 'a' + 10; };
    for (std::size_t i = 0; i < Size; ++i) {
        result[i] = static_cast<std::byte>((digit(hex[2 * i]) * 16) + digit(hex[(2 * i) + 1]));
    }
    return result;
}

const KeyPair ClientKeys{
    .secret = fromHex<KeySize>("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a"),
    .publicKey
    = fromHex<KeySize>("8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a"),
};
const KeyPair ServerKeys{
    .secret = fromHex<KeySize>("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb"),
    .publicKey
    = fromHex<KeySize>("de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f"),
};
const SessionKey KnownKey
    = fromHex<KeySize>("519fb3af2f3f9e310718cf1f8bdec6e26ab64affe730f0f8b43c43b0e8ee52be");

template <std::size_t Size>
Bytes toBytes(const std::array<std::byte, Size>& array)
{
    return {array.begin(), array.end()};
}

} // namespace

TEST(CrossPlatform, TheSessionKeyIsTheSameOnEveryPlatform)
{
    EXPECT_EQ(deriveSessionKey(ClientKeys, ServerKeys.publicKey, Role::Client), KnownKey);
    EXPECT_EQ(deriveSessionKey(ServerKeys, ClientKeys.publicKey, Role::Server), KnownKey);
}

TEST(CrossPlatform, ClientPacketsAreTheSameOnEveryPlatform)
{
    SessionCipher client(KnownKey, Role::Client);

    EXPECT_EQ(seal(client),
        toBytes(fromHex<38>("00000000000000001b8a406762d959990577886afdc9648e82519e0244f88e07d56b"
                            "5f853a98")));
    EXPECT_EQ(seal(client),
        toBytes(fromHex<38>("0000000000000001892b21d70336a262fbc47396016716f0594273660649d222578d"
                            "486ffd36")));
}

TEST(CrossPlatform, ServerPacketsAreTheSameOnEveryPlatform)
{
    SessionCipher server(KnownKey, Role::Server);

    EXPECT_EQ(seal(server),
        toBytes(fromHex<38>("000000000000000053662f3df31e7958d042ad2759a7018c4b74bb559ada24761fe6"
                            "c9e0af1b")));
}

TEST(CrossPlatform, APacketFromAnotherImplementationOpens)
{
    SessionCipher server(KnownKey, Role::Server);

    const Bytes sealed = toBytes(fromHex<38>("00000000000000001b8a406762d959990577886afdc9648e82"
                                             "519e0244f88e07d56b5f853a98"));

    EXPECT_EQ(open(server, sealed).value(), Payload);
}

// --- Passwords ------------------------------------------------------------------

namespace {
constexpr network::crypto::PasswordCost CheapCost{.kibibytes = 8, .passes = 1};
} // namespace

TEST(PasswordHash, SameInputsGiveTheSameHash)
{
    const auto salt = network::crypto::generatePasswordSalt();
    EXPECT_TRUE(network::crypto::samePasswordHash(
        network::crypto::hashPassword("correct horse", salt, CheapCost),
        network::crypto::hashPassword("correct horse", salt, CheapCost)));
}

TEST(PasswordHash, EveryInputChangesTheHash)
{
    const auto salt = network::crypto::generatePasswordSalt();
    const auto hash = network::crypto::hashPassword("correct horse", salt, CheapCost);

    EXPECT_FALSE(network::crypto::samePasswordHash(
        hash, network::crypto::hashPassword("correct horsf", salt, CheapCost)));
    EXPECT_FALSE(network::crypto::samePasswordHash(hash,
        network::crypto::hashPassword(
            "correct horse", network::crypto::generatePasswordSalt(), CheapCost)));
    EXPECT_FALSE(network::crypto::samePasswordHash(hash,
        network::crypto::hashPassword("correct horse", salt, {.kibibytes = 16, .passes = 1})));
    EXPECT_FALSE(network::crypto::samePasswordHash(
        hash, network::crypto::hashPassword("correct horse", salt, {.kibibytes = 8, .passes = 2})));
}

TEST(PasswordHash, MatchesTheReferenceImplementation)
{
    // From libsodium, another implementation: nacl.pwhash.argon2id.kdf(32, b"password",
    // bytes([1]) * 16, opslimit=1, memlimit=8192)
    network::crypto::PasswordSalt salt{};
    salt.fill(std::byte{0x01});
    const auto hash = network::crypto::hashPassword("password", salt, CheapCost);
    constexpr std::array<std::uint8_t, PasswordHashSize> expected{0x55, 0xDB, 0xCE, 0xC5, 0x36,
        0xA7, 0xEA, 0x87, 0x2C, 0xBE, 0x52, 0x8D, 0xFB, 0xBD, 0xB7, 0x77, 0xEF, 0xD2, 0xD1, 0x3A,
        0x7C, 0x8E, 0xA4, 0x51, 0x33, 0xD0, 0xF5, 0x86, 0x84, 0xCA, 0x1F, 0xE7};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(std::to_integer<std::uint8_t>(hash.at(i)), expected.at(i)) << i;
    }
}
