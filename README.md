# UDP File Transfer System

## Overview

This project implements a UDP-based file transfer system as part of a phased approach toward building a reliable data transfer protocol over UDP.

The initial phase focuses on understanding the limitations of raw UDP communication by transmitting files without built-in reliability mechanisms. Subsequent improvements introduce acknowledgment-based reliability and packet loss handling.

The system successfully demonstrates reliable file delivery using ARQ mechanisms under simulated packet loss conditions.

---

## Project Objectives

- Understand the behavior of UDP in file transfer
- Analyze packet loss and its impact on data integrity
- Implement reliability mechanisms over UDP
- Ensure correct file delivery under adverse network conditions

---

## Phase 1: Baseline UDP Implementation

### Description

This phase implements a basic UDP file transfer system without reliability support. It is designed to study the inherent limitations of UDP.

### Functionality

- File transfer using UDP sockets
- Chunk-based data transmission
- Direct writing of received data to file
- No acknowledgments (ACK)
- No retransmission
- No sequencing
- No congestion control

This phase demonstrates raw UDP behavior during file transmission.

---

## Packet Loss Analysis

Since UDP is a connectionless and unreliable protocol, it does not guarantee:

- Packet delivery
- Packet ordering
- Duplicate avoidance

To demonstrate this limitation, packet loss was simulated using Linux Traffic Control (`tc netem`).

### Simulation Details

- Packet loss introduced on the loopback interface
- Large file transfers performed under loss conditions
- No feedback provided to sender on packet loss

### Observations

- Incomplete or corrupted files were received
- Sender remained unaware of lost packets
- No automatic recovery occurred
- File integrity verification confirmed data loss

### Key Findings

- UDP silently drops packets
- No built-in error detection
- No delivery guarantees
- Packet loss can occur even on localhost
- Reliability is essential for large file transfers

This experiment establishes the need for reliability mechanisms.

---

## Phase 2: Reliable UDP Transfer (ARQ Implementation)

### Description

In this phase, reliability was added using Automatic Repeat reQuest (ARQ) techniques.

### Achievements

- Custom packet format implementation
- Handshake protocol for connection setup
- Sequence numbering
- Acknowledgment (ACK) mechanism
- Timeout-based retransmission
- Packet loss detection
- Guaranteed file delivery

### Result

The system successfully transfers files under packet loss conditions while ensuring correctness and completeness.

This confirms the effectiveness of ARQ-based reliability over UDP.

---

## Verification and Testing

- Packet loss simulated using `tc netem`
- File integrity verified using hash comparison
- Multiple test cases with different loss rates
- Successful recovery from packet loss

---

## Requirements

- GCC Compiler
- Linux / Ubuntu
- POSIX Sockets
- Traffic Control (`tc`) for simulation

---

## Compilation

Compile the server:

```bash
gcc udp_server.c -o udp_server
