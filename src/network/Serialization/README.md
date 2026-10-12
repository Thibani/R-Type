# Serialization

| File             | Content                                                    |
|------------------|------------------------------------------------------------|
| `ByteReader.hpp` | `ByteReader`, reads big-endian values without overflowing  |
| `ByteWriter.hpp` | `ByteWriter`, writes big-endian values into a fixed buffer |

Generic tools: they know nothing about the R-Type protocol.

---

## Why not `memcpy` a struct?

```cpp
struct Spawn { std::uint16_t netId; std::uint8_t kind; std::int16_t x; };
send(&spawn, sizeof(spawn)); // NO
```

- **Padding**: the compiler may insert hidden bytes between fields
  (`sizeof(Spawn)` is 6 here, not 5), and how many depends on the compiler.
- **Byte order**: `x` is stored little-endian on x86 but the protocol is
  big-endian. A different CPU would read garbage.
- **Safety**: reading a struct straight from a received buffer trusts the
  sender completely.

So every field is written and read one by one, in big-endian order.

---

## Sticky errors

Both classes stop at the first overflow and ignore every following
operation. You read a whole message, then check once:

```cpp
network::ByteReader reader(datagram);
const auto netId = reader.read<std::uint16_t>();
const auto x = reader.read<std::int16_t>();
const auto y = reader.read<std::int16_t>();
if (!reader.ok()) {
    return; // truncated: netId, x and y are 0 and must not be used
}
```

| Method               | Meaning                                              |
|----------------------|------------------------------------------------------|
| `read<T>()`          | Reads an integer, or returns 0 if it does not fit    |
| `readBytes(span)`    | Copies raw bytes                                     |
| `remaining()`        | Bytes left (0 once failed)                           |
| `ok()`               | No read overflowed                                   |
| `finished()`         | `ok()` and every byte was consumed                   |
| `write(value)`       | Writes an integer (`ByteWriter`)                     |
| `size()`             | Bytes written so far (`ByteWriter`)                  |

Neither class allocates nor throws: they are safe on hostile input.

Tests: [`tests/network/Serialization/`](../../../tests/network/Serialization).
