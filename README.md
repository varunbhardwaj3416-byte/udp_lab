# Enhancing File Transfer Efficiency Beyond TCP Using a Custom UDP-Based Protocol

A custom application-layer file transfer protocol built on UDP in C, implementing reliability, congestion control, and a full security stack from scratch.

## Features

| Layer | Mechanism |
|---|---|
| Reliability | Selective Repeat ARQ, Sliding Window (64 packets) |
| Flow/Congestion | Slow Start + AIMD, EWMA RTT, Karn's Algorithm |
| Integrity | CRC32 (corruption detection) + HMAC-SHA256 (authentication) |
| Confidentiality | AES-256-GCM authenticated encryption |
| Key Exchange | Diffie-Hellman (RFC 3526 Group 14, 2048-bit) + HKDF session key derivation |

## Build

```bash
# Dependencies: OpenSSL 3.x, GCC, Make
sudo apt install libssl-dev   # Ubuntu/Debian
make
```

Binaries are placed in `build/`.

## Usage

```bash
# Terminal 1 — start receiver
./build/receiver <port> <output_directory>

# Terminal 2 — send a file
./build/sender <dest_ip> <port> <file_path>
```

## Project Structure

```
.
├── include/
│   ├── packet.h     # Packet structure and wire format
│   ├── crypto.h     # AES-GCM, HMAC, CRC32, DH, HKDF
│   ├── arq.h        # Sliding window ARQ, RTT, congestion control
│   └── socket.h     # UDP socket abstraction
├── src/
│   ├── packet.c     # Serialisation / deserialisation
│   ├── crypto.c     # OpenSSL wrappers
│   ├── arq.c        # ARQ state machine + congestion control
│   ├── socket.c     # sendto / recvfrom wrappers
│   ├── sender.c     # Sender main loop
│   └── receiver.c   # Receiver main loop
└── Makefile
```

## Packet Wire Format

```
[4]  seq_num
[4]  crc32          -- over all fields except crc32 and hmac
[1]  type           -- DATA / ACK / SACK / FIN / HANDSHAKE
[12] nonce          -- AES-GCM nonce (random per packet)
[16] gcm_tag        -- AES-GCM authentication tag
[N]  payload        -- encrypted ciphertext (N <= 1400 bytes)
[32] hmac           -- HMAC-SHA256 over all preceding bytes
```

## Author

Varun Bhardwaj — B.Tech CSE, Manipal University Jaipur (2427030406)
