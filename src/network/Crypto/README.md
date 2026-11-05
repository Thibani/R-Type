# Crypto

| File         | Content                                                          |
|--------------|------------------------------------------------------------------|
| `Crypto.hpp` | Key exchange, `SessionCipher` (seal / open), `ReplayWindow`      |
| `SealedDatagram.hpp` | `SessionCipher` applied to a whole datagram, header kept readable |

Protects a session against reading, modifying, forging and replaying packets
(issue #97). The cryptography itself comes from
[Monocypher](https://monocypher.org), never from home-made code.

---

## How it works

1. **Key exchange (X25519).** Each side creates a key pair and sends its public
   key. Each side combines its secret with the other's public key and gets the
   same result, which never travels on the network.
2. **Session key (BLAKE2b).** That result is hashed with both public keys.
3. **Sealing (XChaCha20-Poly1305).** The payload is encrypted. A 16-byte tag
   covers the payload and the header: any modification makes `open()` fail.

```
sealed payload = counter (8 bytes) | encrypted payload | tag (16 bytes)
```

---

## Why a 64-bit counter?

The nonce must never be used twice with one key: that would leak information
about the two payloads. The 16-bit `sequence` of the header wraps after 65,536
packets (about 18 minutes at 60 packets per second), so it cannot be the nonce.

```
nonce (24 bytes) = sender role (1) | counter (8) | zeros (15)
```

- **Session**: each session has its own key.
- **Direction**: the role is part of the nonce, since both sides count from 0.
- **Packet**: the counter never goes back.

The counter also rejects replayed packets. `ReplayWindow` accepts a packet that
was never received, up to 64 packets behind the newest one, because UDP can
reorder packets. A counter is recorded only after its tag is verified, so a
forged packet cannot block real ones.

---

## Choices

| Choice                     | Rejected alternative | Why                                                                 |
|----------------------------|----------------------|---------------------------------------------------------------------|
| Monocypher                 | libsodium            | 1 C file to compile; libsodium needs autotools or its MSVC project  |
| Monocypher                 | OpenSSL              | Much larger than the three algorithms needed here                   |
| One key, role in the nonce | One key per direction | A single value to derive and store                                 |
| Separate 64-bit counter    | Wider `sequence`     | The header and `ReliableChannel` stay unchanged                     |

## Known limit

The key exchange alone does not prove who the peer is: an active attacker placed
between the client and the server could run one exchange with each of them.
