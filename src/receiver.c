#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "socket.h"
#include "packet.h"
#include "crypto.h"
#include "arq.h"

#define MAX_BUF_SIZE  (sizeof(Packet) + 64)

/* Verify CRC32, HMAC, and GCM tag in sequence; return 0 if all pass */
static int verify_packet(const Packet *pkt, const uint8_t *raw, size_t raw_len,
                          const SessionKeys *keys)
{
    /* 1. CRC32 check */
    uint32_t expected_crc = crc32_compute(raw + CRC32_SIZE,
                                          raw_len - CRC32_SIZE - HMAC_LEN);
    if (expected_crc != pkt->crc32) {
        fprintf(stderr, "[receiver] CRC mismatch seq=%u — dropped\n", pkt->seq_num);
        return -1;
    }

    /* 2. HMAC-SHA256 check (constant-time) */
    uint8_t computed_hmac[HMAC_LEN];
    if (crypto_hmac_sha256(keys->mac_key, HMAC_KEY_LEN,
                           raw, raw_len - HMAC_LEN,
                           computed_hmac) != 0)
        return -1;

    if (crypto_memcmp(computed_hmac, pkt->hmac, HMAC_LEN) != 0) {
        fprintf(stderr, "[receiver] HMAC mismatch seq=%u — dropped\n", pkt->seq_num);
        return -1;
    }

    /* 3. AES-GCM decryption — GCM tag verified internally by OpenSSL */
    uint8_t plaintext[MAX_PAYLOAD_SIZE];
    if (crypto_aes_gcm_decrypt(keys->enc_key, pkt->nonce,
                                pkt->payload, pkt->payload_len,
                                pkt->gcm_tag,
                                plaintext) != 0) {
        fprintf(stderr, "[receiver] GCM auth failed seq=%u — dropped\n", pkt->seq_num);
        return -1;
    }

    return 0;
}

/* Send a cumulative ACK for seq */
static void send_ack(UdpSocket *sock, uint32_t seq)
{
    Packet ack;
    memset(&ack, 0, sizeof(ack));
    ack.type    = PKT_ACK;
    ack.seq_num = seq;

    uint8_t buf[MAX_BUF_SIZE];
    size_t  len = packet_serialize(&ack, buf, sizeof(buf));
    sock_send(sock, buf, len);
}

/* Send a SACK with bitmap of received out-of-order packets */
static void send_sack(UdpSocket *sock, uint32_t base_seq, uint64_t bitmap)
{
    Packet sack;
    memset(&sack, 0, sizeof(sack));
    sack.type        = PKT_SACK;
    sack.seq_num     = base_seq;
    sack.payload_len = sizeof(bitmap);
    memcpy(sack.payload, &bitmap, sizeof(bitmap));

    uint8_t buf[MAX_BUF_SIZE];
    size_t  len = packet_serialize(&sack, buf, sizeof(buf));
    sock_send(sock, buf, len);
}

/*
 * DH handshake on receiver side:
 * receive sender's public key, send ours, derive session keys.
 */
static int do_handshake(UdpSocket *sock, SessionKeys *keys)
{
    DhCtx  *ctx = dh_ctx_new();
    if (!ctx) return -1;

    uint8_t buf[MAX_BUF_SIZE];
    int n = sock_recv(sock, buf, sizeof(buf), 10000);
    if (n <= 0) goto fail;

    Packet hpkt;
    if (packet_deserialize(&hpkt, buf, (size_t)n) != 0) goto fail;
    if (hpkt.type != PKT_HANDSHAKE) goto fail;

    /* Generate our keypair and send public key back */
    uint8_t pubkey[512];
    size_t  pubkey_len = sizeof(pubkey);
    if (dh_generate_keypair(ctx, pubkey, &pubkey_len) != 0) goto fail;

    Packet rpkt;
    memset(&rpkt, 0, sizeof(rpkt));
    rpkt.type        = PKT_HANDSHAKE;
    rpkt.seq_num     = 0;
    rpkt.payload_len = (uint16_t)pubkey_len;
    memcpy(rpkt.payload, pubkey, pubkey_len);

    size_t len = packet_serialize(&rpkt, buf, sizeof(buf));
    if (sock_send(sock, buf, len) != 0) goto fail;

    /* Compute shared secret from sender's public key */
    uint8_t shared[512];
    size_t  shared_len = sizeof(shared);
    if (dh_compute_shared(ctx, hpkt.payload, hpkt.payload_len,
                          shared, &shared_len) != 0)
        goto fail;

    if (hkdf_derive_session_keys(shared, shared_len, keys) != 0) goto fail;

    dh_ctx_free(ctx);
    return 0;
fail:
    dh_ctx_free(ctx);
    return -1;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <output_dir>\n", argv[0]);
        return 1;
    }

    uint16_t    port       = (uint16_t)atoi(argv[1]);
    const char *output_dir = argv[2];

    UdpSocket sock;
    if (sock_create_receiver(&sock, port) != 0) return 1;
    printf("[receiver] listening on port %u\n", port);

    SessionKeys keys;
    if (do_handshake(&sock, &keys) != 0) {
        fprintf(stderr, "[receiver] handshake failed\n");
        return 1;
    }
    printf("[receiver] handshake complete, session keys established\n");

    char out_path[512];
    snprintf(out_path, sizeof(out_path), "%s/received_file", output_dir);
    FILE *fp = fopen(out_path, "wb");
    if (!fp) { perror("fopen output"); return 1; }

    ReceiverARQ arq;
    arq_receiver_init(&arq);

    uint8_t raw[MAX_BUF_SIZE];
    uint8_t plaintext[MAX_PAYLOAD_SIZE];
    int     running = 1;

    while (running) {
        int n = sock_recv(&sock, raw, sizeof(raw), 30000);
        if (n <= 0) {
            fprintf(stderr, "[receiver] timeout or error — exiting\n");
            break;
        }

        Packet pkt;
        if (packet_deserialize(&pkt, raw, (size_t)n) != 0)
            continue;

        if (pkt.type == PKT_FIN) {
            printf("[receiver] FIN received — transfer complete\n");
            Packet finack;
            memset(&finack, 0, sizeof(finack));
            finack.type = PKT_FINACK;
            uint8_t fbuf[MAX_BUF_SIZE];
            size_t  flen = packet_serialize(&finack, fbuf, sizeof(fbuf));
            sock_send(&sock, fbuf, flen);
            running = 0;
            break;
        }

        if (pkt.type != PKT_DATA)
            continue;

        /* Validate CRC, HMAC, GCM */
        if (verify_packet(&pkt, raw, (size_t)n, &keys) != 0)
            continue;

        printf("[receiver] received seq=%u\n", pkt.seq_num);

        if (arq_receiver_insert(&arq, &pkt) != 0)
            continue;   /* duplicate or out-of-window */

        /* Drain in-order packets to disk */
        Packet drained[WINDOW_SIZE];
        int count = arq_receiver_drain(&arq, drained, WINDOW_SIZE);
        for (int i = 0; i < count; i++) {
            /* Decrypt for writing */
            size_t pt_len = drained[i].payload_len;
            if (crypto_aes_gcm_decrypt(keys.enc_key, drained[i].nonce,
                                       drained[i].payload, pt_len,
                                       drained[i].gcm_tag,
                                       plaintext) == 0) {
                fwrite(plaintext, 1, pt_len, fp);
            }
        }

        /* Send ACK or SACK */
        uint64_t bitmap = arq_receiver_sack_bitmap(&arq);
        if (bitmap == 0) {
            send_ack(&sock, arq.expected - 1);
        } else {
            send_sack(&sock, arq.expected, bitmap);
        }
    }

    fclose(fp);
    sock_close(&sock);
    printf("[receiver] file saved to %s\n", out_path);
    return 0;
}
