#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>

namespace network {

/// Writes big-endian values into a caller-provided buffer.
///
/// Like `ByteReader`, the writer is "sticky": the first write that does not
/// fit marks it as failed, and every following write is ignored. Check
/// `ok()` once at the end.
///
///     std::array<std::byte, 64> buffer{};
///     ByteWriter writer(buffer);
///     writer.write<std::uint16_t>(netId);
///     writer.write<std::int16_t>(x);
///     if (writer.ok()) {
///         send(std::span(buffer).first(writer.size()));
///     }
///
/// The writer never allocates and never throws.
class ByteWriter {
public:
    explicit constexpr ByteWriter(std::span<std::byte> buffer) noexcept
        : _buffer(buffer)
    {
    }

    /// Writes an integer in big-endian order.
    template <std::integral T>
    constexpr void write(T value) noexcept
    {
        if (!canWrite(sizeof(T))) {
            return;
        }

        // Wide unsigned: negative values wrap to their two's complement bytes.
        const auto bits = static_cast<std::uint64_t>(value);
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            const auto shift = (sizeof(T) - 1 - i) * 8;
            _buffer[_offset + i] = static_cast<std::byte>((bits >> shift) & 0xFFU);
        }
        _offset += sizeof(T);
    }

    constexpr void writeBytes(std::span<const std::byte> source) noexcept
    {
        if (!canWrite(source.size())) {
            return;
        }
        for (std::size_t i = 0; i < source.size(); ++i) {
            _buffer[_offset + i] = source[i];
        }
        _offset += source.size();
    }

    /// Number of bytes written so far.
    [[nodiscard]]
    constexpr std::size_t size() const noexcept
    {
        return _offset;
    }

    /// True if every write fitted in the buffer.
    [[nodiscard]]
    constexpr bool ok() const noexcept
    {
        return !_failed;
    }

private:
    constexpr bool canWrite(std::size_t size) noexcept
    {
        if (_failed || size > _buffer.size() - _offset) {
            _failed = true;
            return false;
        }
        return true;
    }

    std::span<std::byte> _buffer;
    std::size_t _offset{0};
    bool _failed{false};
};

} // namespace network
