# Transport

| File            | Content                                              |
|-----------------|------------------------------------------------------|
| `UdpSocket.hpp` | Non-blocking UDP socket, `Endpoint`, `Datagram`      |
| `NetworkThread.hpp` | A UDP socket served by its own thread (the server) |

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

---

## NetworkThread: the server's socket on its own thread

The server must never wait for clients (the subject requires a multithreaded
server), so its socket lives on a thread of its own:

```cpp
auto network = network::NetworkThread::open(4242); // binds, starts the thread
// Game thread, once per frame:
for (const auto& datagram : (*network)->takeReceived()) {
    // decode(datagram.bytes, Direction::ClientToServer), sender datagram.from
}
(*network)->send(bytes, to); // copied, sent by the network thread
network->reset(); // sends what is queued, then joins the thread
```

| Data             | Used by        | Protection                       |
|------------------|----------------|----------------------------------|
| Socket           | network thread | used by that thread only         |
| Inbox            | both           | `std::mutex`, emptied by one swap |
| Datagrams to send | both          | `asio::post`, thread-safe        |

At most 1024 datagrams wait in the inbox; beyond, new ones are dropped and
counted (`droppedCount()`). Receive errors are skipped like `UdpSocket`'s
(`isTransientReceiveError()`), others are logged: the thread never stops
receiving until it is destroyed.
