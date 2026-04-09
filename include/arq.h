#ifndef ARQ_H
#define ARQ_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "packet.h"

#define WINDOW_SIZE      64    /* max unacknowledged packets in flight */
#define MAX_RETRIES      10
#define SACK_BITMAP_LEN  8     /* 64-bit bitmap covers WINDOW_SIZE slots */

/* One slot in the sender's window */
typedef struct {
    Packet   pkt;
    bool     in_use;
    bool     acked;
    int      retries;
    struct timespec sent_at;   /* for RTT sampling */
} SendSlot;

/* One slot in the receiver's reorder buffer */
typedef struct {
    Packet   pkt;
    bool     received;
} RecvSlot;

/* Sender-side ARQ state */
typedef struct {
    SendSlot  window[WINDOW_SIZE];
    uint32_t  base;            /* oldest unacked seq */
    uint32_t  next_seq;        /* next seq to assign */
    uint32_t  cwnd;            /* congestion window (in packets) */
    uint32_t  ssthresh;        /* slow start threshold */
    long      rto_ms;          /* current retransmit timeout (ms) */
    double    srtt_ms;         /* smoothed RTT */
    double    rttvar_ms;       /* RTT variance */
    bool      rtt_initialized;
} SenderARQ;

/* Receiver-side ARQ state */
typedef struct {
    RecvSlot  buffer[WINDOW_SIZE];
    uint32_t  expected;        /* next in-order seq expected */
} ReceiverARQ;

/* --- Sender --- */
void     arq_sender_init(SenderARQ *arq);
int      arq_sender_add(SenderARQ *arq, const Packet *pkt);   /* returns slot index or -1 if window full */
void     arq_sender_ack(SenderARQ *arq, uint32_t seq);
void     arq_sender_sack(SenderARQ *arq, uint32_t base_seq, uint64_t bitmap);
bool     arq_sender_window_full(const SenderARQ *arq);
int      arq_sender_get_timed_out(SenderARQ *arq, long now_ms, uint32_t *out_seqs, int max);

/* --- Receiver --- */
void     arq_receiver_init(ReceiverARQ *arq);
int      arq_receiver_insert(ReceiverARQ *arq, const Packet *pkt);  /* 0=ok, -1=dup/out-of-window */
/* Drains contiguous in-order packets; returns count drained */
int      arq_receiver_drain(ReceiverARQ *arq, Packet *out, int max_out);
/* Builds SACK bitmap for out-of-order holes */
uint64_t arq_receiver_sack_bitmap(const ReceiverARQ *arq);

/* --- RTT / RTO (Jacobson/Karels + Karn's Algorithm) --- */
void     rtt_update(SenderARQ *arq, uint32_t seq, long now_ms);  /* call only on non-retransmitted ACKs */
void     rto_backoff(SenderARQ *arq);                             /* exponential backoff on timeout */

/* --- Congestion control (Slow Start + AIMD) --- */
void     cc_on_ack(SenderARQ *arq);        /* advance cwnd */
void     cc_on_timeout(SenderARQ *arq);    /* reset cwnd, halve ssthresh */
void     cc_on_triple_dup_ack(SenderARQ *arq); /* fast retransmit/recovery */

#endif /* ARQ_H */
