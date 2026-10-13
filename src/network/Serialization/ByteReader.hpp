#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>

namespace network {

/// Reads big-endian values from a buffer, without ever reading past its end.
///
/// The reader is "sticky": the first read that would overflow the buffer
/// marks it as failed, and every following read returns 0 without moving.
/// Parsing code can therefore read a whole message and check `ok()` once:
///
///     ByteReader reader(datagram);
///     const auto netId = reader.read<std::uint16_t>();
///     const auto x = reader.read<std::int16_t>();
///     if (!reader.ok()) {
///         return; // truncated message, values above are meaningless
///     }
///
/// The reader never allocates and never throws.
class ByteReader {
public:
    explicit constexpr ByteReader(std::span<const std::byte> buffer) noexcept
        : _buffer(buffer)
    {
    }

    /// Reads an integer stored in big-endian order.
    /// Returns 0 and fails the reader if fewer than `sizeof(T)` bytes remain.
    template <std::integral T>
    [[nodiscard]]
    constexpr T read() noexcept
    {
        if (!canRead(sizeof(T))) {
            return T{};
        }

        std::uint64_t value{}; // Wide unsigned: no promotion to signed int while shifting
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            value = (value << 8U) | std::to_integer<std::uint64_t>(_buffer[_offset + i]);
        }
        _offset += sizeof(T);
        return static_cast<T>(value);
    }

    /// Copies `destination.size()` bytes into `destination`.
    /// Returns false (and fails the reader) if not enough bytes remain.
    constexpr bool readBytes(std::span<std::byte> destination) noexcept
    {
        if (!canRead(destination.size())) {
            return false;
        }
        for (std::size_t i = 0; i < destination.size(); ++i) {
            destination[i] = _buffer[_offset + i];
        }
        _offset += destination.size();
        return true;
    }

    /// Number of bytes not read yet (0 once the reader failed).
    [[nodiscard]]
    constexpr std::size_t remaining() const noexcept
    {
        return _failed ? 0 : _buffer.size() - _offset;
    }

    /// True if no read has overflowed the buffer.
    [[nodiscard]]
    constexpr bool ok() const noexcept
    {
        return !_failed;
    }

    /// True if every byte was read and no read failed.
    [[nodiscard]]
    constexpr bool finished() const noexcept
    {
        return ok() && remaining() == 0;
    }

private:
    constexpr bool canRead(std::size_t size) noexcept
    {
        if (_failed || size > _buffer.size() - _offset) {
            _failed = true;
            return false;
        }
        return true;
    }

    std::span<const std::byte> _buffer;
    std::size_t _offset{0};
    bool _failed{false};
};

} // namespace network
