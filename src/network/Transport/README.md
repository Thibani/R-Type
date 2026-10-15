# Transport

| File            | Content                                              |
|-----------------|------------------------------------------------------|
| `UdpSocket.hpp` | Non-blocking UDP socket, `Endpoint`, `Datagram`      |

The only code that touches the operating system's sockets, through
[standalone Asio](https://think-async.com/Asio/) so the same code builds on
Linux and Windows. It moves raw bytes: turning them into packets is
[`Protocol/`](../Protocol/README.md)'s job.

Nothing ever blocks and nothing throws: every call returns a
`std::expected<..., std::error_code>`.

---

## Server

```cpp
auto socket = network::UdpSocket::open(4242);
if (!socket) {
    std::println(stderr, "cannot bind: {}", socket.error().message());
    return 84;
}
```

## Client

```cpp
auto socket = network::UdpSocket::open(0); // any free local port
const network::Endpoint server{asio::ip::make_address("127.0.0.1"), 4242};
socket->sendTo(std::span(buffer).first(*size), server);
```

## Receiving, once per tick

```cpp
DatagramBuffer buffer{};
while (true) {
    auto received = socket->receive(buffer);
    if (!received) {
        log(received.error().message()); // the socket itself failed
        break;
    }
    if (!*received) {
        break; // nothing left to read this tick
    }
    const auto datagram = std::span(buffer).first((*received)->size);
    // decode(datagram, Direction::ClientToServer), then handle (*received)->from
}
```

ICMP "port unreachable" replies (a client that disappeared) and oversized
datagrams are skipped silently: they are not failures of the socket.
