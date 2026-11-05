#include "network/Crypto/SealedDatagram.hpp"

#include "network/Protocol/Protocol.hpp"
#include "network/Serialization/ByteWriter.hpp"

#include <algorithm>
#include <cstdint>

namespace network::crypto {

using protocol::EntityStateSize;
using protocol::HeaderSize;
using protocol::MaxDatagramSize;
using protocol::MaxSnapshotEntities;
using protocol::SealOverhead;
using protocol::SnapshotHeaderSize;

static_assert(SealOverhead == Overhead);
// The largest packet must still fit in a datagram once sealed
static_assert(HeaderSize + SnapshotHeaderSize + (MaxSnapshotEntities * EntityStateSize) + Overhead
    <= MaxDatagramSize);

namespace {

constexpr std::size_t PayloadSizeOffset = 6; // §3

/// Copies the header of `datagram` into `output`, with a new `payloadSize`.
void writeHeader(
    std::span<const std::byte> datagram, std::span<std::byte> output, std::size_t payloadSize)
{
    std::ranges::copy(datagram.first(HeaderSize), output.begin());
    ByteWriter writer(output.subspan(PayloadSizeOffset, sizeof(std::uint16_t)));
    writer.write(static_cast<std::uint16_t>(payloadSize));
}

} // namespace

std::expected<std::size_t, SealError> sealDatagram(
    SessionCipher& cipher, std::span<const std::byte> datagram, std::span<std::byte> output)
{
    if (datagram.size() < HeaderSize) {
        return std::unexpected(SealError::TooSmall);
    }
    if (output.size() < datagram.size() + Overhead) {
        return std::unexpected(SealError::BufferTooSmall);
    }

    const std::span<const std::byte> payload = datagram.subspan(HeaderSize);
    writeHeader(datagram, output, payload.size() + Overhead);

    // The final header is what is protected: the receiver sees those exact bytes
    const auto sealed = cipher.seal(output.first(HeaderSize), payload, output.subspan(HeaderSize));
    if (!sealed) {
        return std::unexpected(sealed.error());
    }
    return HeaderSize + *sealed;
}

std::expected<std::size_t, OpenError> openDatagram(
    SessionCipher& cipher, std::span<const std::byte> datagram, std::span<std::byte> output)
{
    if (datagram.size() < HeaderSize) {
        return std::unexpected(OpenError::TooSmall);
    }
    if (output.size() < HeaderSize) {
        return std::unexpected(OpenError::BufferTooSmall);
    }

    const auto opened = cipher.open(
        datagram.first(HeaderSize), datagram.subspan(HeaderSize), output.subspan(HeaderSize));
    if (!opened) {
        return std::unexpected(opened.error());
    }
    writeHeader(datagram, output, *opened);
    return HeaderSize + *opened;
}

} // namespace network::crypto
