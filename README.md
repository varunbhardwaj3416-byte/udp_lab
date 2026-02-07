UDP File Transfer – Phase 1 (Baseline Implementation)
Overview

This project implements a basic UDP-based file transfer system as an initial phase toward building a reliable data transfer protocol over UDP. The current implementation focuses on transmitting file data using UDP sockets without acknowledgments, retransmission, or flow control, in order to study the inherent limitations of UDP.

Current Functionality

File transfer using UDP sockets

Chunk-based transmission of file data

Receiver writes incoming data directly to file

No acknowledgment (ACK) mechanism

No retransmission or sequencing

No congestion or flow control

This phase demonstrates raw UDP behavior during file transmission.

Packet Loss Observation

Since UDP is a connectionless and unreliable protocol, it does not guarantee packet delivery, ordering, or duplication avoidance. To demonstrate this limitation, packet loss was simulated at the operating system level using Linux Traffic Control (tc netem).

Packet loss was introduced on the loopback interface during large file transfers. Under these conditions, the receiver obtained incomplete or corrupted files, while the sender remained unaware of the loss. This occurred because UDP provides no built-in feedback mechanism to detect missing packets.

File integrity was verified using cryptographic hash comparison, which confirmed data loss when packet loss was present.

Key Observation

UDP silently drops packets when loss occurs.

The sender does not receive any error or indication of missing data.

Large file transfers are especially vulnerable without reliability mechanisms.

Packet loss can occur even on localhost and does not depend on Wi-Fi or internet instability.

This experiment establishes the need for reliability mechanisms such as acknowledgments and retransmissions.

Scope of This Phase

This phase is intentionally limited to:

Demonstrating UDP file transfer

Observing packet loss behavior

Establishing motivation for reliability design

No attempt is made in this phase to correct or recover lost packets.

Future Work

Subsequent phases will extend this implementation to include:

Packet sequencing

Acknowledgment (ACK) mechanisms

Timeout and retransmission logic


