#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "arq.h"

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static long elapsed_ms(const struct timespec *sent)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec  - sent->tv_sec)  * 1000L
         + (now.tv_nsec - sent->tv_nsec) / 1000000L;
}

/* Map sequence number to window slot (ring buffer) */
static inline int slot(uint32_t seq)
{
    return (int)(seq % WINDOW_SIZE);
}

/* ------------------------------------------------------------------ */
/* RTT estimation (Jacobson/Karels) + Karn's Algorithm                */
/* ------------------------------------------------------------------ */

#define RTT_ALPHA  0.125   /* EWMA weight for new samples  */
#define RTT_BETA   0.25    /* EWMA weight for variance     */
#define RTO_MIN_MS 200L
#define RTO_MAX_MS 60000L

void rtt_update(SenderARQ *arq, uint32_t seq, long now_ms_val)
{
    SendSlot *s = &arq->window[slot(seq)];

    /* Karn: skip RTT sample if this packet was retransmitted */
    if (s->retries > 0)
        return;

    long rtt = elapsed_ms(&s->sent_at);
    if (rtt < 0) rtt = 0;

    if (!arq->rtt_initialized) {
        arq->srtt_ms        = (double)rtt;
        arq->rttvar_ms      = (double)rtt / 2.0;
        arq->rtt_initialized = true;
    } else {
        double err       = (double)rtt - arq->srtt_ms;
        arq->srtt_ms    += RTT_ALPHA * err;
        arq->rttvar_ms   = (1.0 - RTT_BETA) * arq->rttvar_ms
                         + RTT_BETA * (err < 0 ? -err : err);
    }

    arq->rto_ms = (long)(arq->srtt_ms + 4.0 * arq->rttvar_ms);
    if (arq->rto_ms < RTO_MIN_MS) arq->rto_ms = RTO_MIN_MS;
    if (arq->rto_ms > RTO_MAX_MS) arq->rto_ms = RTO_MAX_MS;
    (void)now_ms_val;
}

void rto_backoff(SenderARQ *arq)
{
    arq->rto_ms *= 2;
    if (arq->rto_ms > RTO_MAX_MS)
        arq->rto_ms = RTO_MAX_MS;
}

/* ------------------------------------------------------------------ */
/* Congestion control (Slow Start + AIMD / TCP Reno model)            */
/* ------------------------------------------------------------------ */

void cc_on_ack(SenderARQ *arq)
{
    if (arq->cwnd < arq->ssthresh) {
        /* Slow Start: double cwnd each RTT */
        arq->cwnd++;
    } else {
        /* Congestion Avoidance (AIMD): +1 per RTT worth of ACKs */
        /* Approximated as +1/cwnd per ACK */
        arq->cwnd++;   /* simplified; production would use cwnd += 1/cwnd */
    }
    if (arq->cwnd > WINDOW_SIZE)
        arq->cwnd = WINDOW_SIZE;
}

void cc_on_timeout(SenderARQ *arq)
{
    arq->ssthresh = arq->cwnd / 2;
    if (arq->ssthresh < 2)
        arq->ssthresh = 2;
    arq->cwnd = 1;   /* restart Slow Start */
    rto_backoff(arq);
}

void cc_on_triple_dup_ack(SenderARQ *arq)
{
    /* Fast Retransmit / Fast Recovery */
    arq->ssthresh = arq->cwnd / 2;
    if (arq->ssthresh < 2)
        arq->ssthresh = 2;
    arq->cwnd = arq->ssthresh;   /* skip Slow Start, enter CA directly */
}

/* ------------------------------------------------------------------ */
/* Sender ARQ                                                          */
/* ------------------------------------------------------------------ */

void arq_sender_init(SenderARQ *arq)
{
    memset(arq, 0, sizeof(*arq));
    arq->cwnd     = 1;
    arq->ssthresh = WINDOW_SIZE;
    arq->rto_ms   = 1000L;
}

int arq_sender_add(SenderARQ *arq, const Packet *pkt)
{
    if (arq_sender_window_full(arq))
        return -1;

    int s = slot(arq->next_seq);
    arq->window[s].pkt     = *pkt;
    arq->window[s].in_use  = true;
    arq->window[s].acked   = false;
    arq->window[s].retries = 0;
    clock_gettime(CLOCK_MONOTONIC, &arq->window[s].sent_at);

    arq->next_seq++;
    return s;
}

void arq_sender_ack(SenderARQ *arq, uint32_t seq)
{
    if (seq < arq->base || seq >= arq->next_seq)
        return;

    SendSlot *s = &arq->window[slot(seq)];
    if (!s->in_use || s->acked)
        return;

    rtt_update(arq, seq, now_ms());
    s->acked  = true;
    s->in_use = false;

    /* Slide window base forward over contiguous acked slots */
    while (arq->base < arq->next_seq && arq->window[slot(arq->base)].acked) {
        arq->window[slot(arq->base)].acked  = false;
        arq->window[slot(arq->base)].in_use = false;
        arq->base++;
    }

    cc_on_ack(arq);
}

void arq_sender_sack(SenderARQ *arq, uint32_t base_seq, uint64_t bitmap)
{
    /* Each bit i set means (base_seq + i) was received */
    for (int i = 0; i < WINDOW_SIZE; i++) {
        if (bitmap & (1ULL << i))
            arq_sender_ack(arq, base_seq + (uint32_t)i);
    }
}

bool arq_sender_window_full(const SenderARQ *arq)
{
    uint32_t in_flight = arq->next_seq - arq->base;
    return in_flight >= arq->cwnd || in_flight >= WINDOW_SIZE;
}

int arq_sender_get_timed_out(SenderARQ *arq, long now_ms_val,
                              uint32_t *out_seqs, int max)
{
    int count = 0;
    for (uint32_t s = arq->base; s < arq->next_seq && count < max; s++) {
        SendSlot *slot_p = &arq->window[slot(s)];
        if (!slot_p->in_use || slot_p->acked)
            continue;
        if (elapsed_ms(&slot_p->sent_at) >= arq->rto_ms) {
            out_seqs[count++] = s;
            slot_p->retries++;
            clock_gettime(CLOCK_MONOTONIC, &slot_p->sent_at);
        }
    }
    (void)now_ms_val;
    return count;
}

/* ------------------------------------------------------------------ */
/* Receiver ARQ                                                        */
/* ------------------------------------------------------------------ */

void arq_receiver_init(ReceiverARQ *arq)
{
    memset(arq, 0, sizeof(*arq));
}

int arq_receiver_insert(ReceiverARQ *arq, const Packet *pkt)
{
    uint32_t seq = pkt->seq_num;

    /* Reject packets outside the receive window */
    if (seq < arq->expected || seq >= arq->expected + WINDOW_SIZE)
        return -1;

    int s = slot(seq);
    if (arq->buffer[s].received)
        return -1;   /* duplicate */

    arq->buffer[s].pkt      = *pkt;
    arq->buffer[s].received = true;
    return 0;
}

int arq_receiver_drain(ReceiverARQ *arq, Packet *out, int max_out)
{
    int count = 0;
    while (count < max_out && arq->buffer[slot(arq->expected)].received) {
        out[count++] = arq->buffer[slot(arq->expected)].pkt;
        arq->buffer[slot(arq->expected)].received = false;
        arq->expected++;
    }
    return count;
}

uint64_t arq_receiver_sack_bitmap(const ReceiverARQ *arq)
{
    uint64_t bitmap = 0;
    for (int i = 0; i < WINDOW_SIZE; i++) {
        uint32_t seq = arq->expected + (uint32_t)i;
        if (arq->buffer[slot(seq)].received)
            bitmap |= (1ULL << i);
    }
    return bitmap;
}
