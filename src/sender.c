#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "socket.h"
#include "packet.h"
#include "crypto.h"
#include "arq.h"

#define MAX_BUF_SIZE  (sizeof(Packet) + 64)

/* Send a fully constructed packet over the socket */
static int send_packet(UdpSocket *sock, const Packet *pkt,
                       const SessionKeys *keys)
{
    uint8_t buf[MAX_BUF_SIZE];
    size_t  len = packet_serialize(pkt, buf, sizeof(buf));
    if (len == 0) return -1;
    return sock_send(sock, buf, len);
}

/*
 * Build and send a DATA packet for one chunk of file data.
 * Encrypts plaintext with AES-256-GCM, stamps CRC32 and HMAC.
 */
static int build_data_packet(Packet *pkt, uint32_t seq,
                              const uint8_t *plaintext, size_t pt_len,
                              const SessionKeys *keys)
{
    memset(pkt, 0, sizeof(*pkt));
    pkt->seq_num     = seq;
    pkt->type        = PKT_DATA;
    pkt->payload_len = (uint16_t)pt_len;

    /* Unique nonce per packet */
    if (crypto_random_bytes(pkt->nonce, GCM_NONCE_LEN) != 0)
        return -1;

    /* Encrypt plaintext into pkt->payload; tag goes into pkt->gcm_tag */
    if (crypto_aes_gcm_encrypt(keys->enc_key, pkt->nonce,
                                plaintext, pt_len,
                                pkt->payload, pkt->gcm_tag) != 0)
        return -1;

    /* CRC32 over everything except crc32 field and hmac */
    uint8_t tmp[MAX_BUF_SIZE];
    size_t  tmp_len = packet_serialize(pkt, tmp, sizeof(tmp));
    pkt->crc32 = crc32_compute(tmp + CRC32_SIZE, tmp_len - CRC32_SIZE - HMAC_LEN);

    /* HMAC over full packet excluding the trailing hmac field */
    tmp_len = packet_serialize(pkt, tmp, sizeof(tmp));
    if (crypto_hmac_sha256(keys->mac_key, HMAC_KEY_LEN,
                           tmp, tmp_len - HMAC_LEN,
                           pkt->hmac) != 0)
        return -1;

    return 0;
}

/*
 * Perform DH handshake: send our public key, receive peer's,
 * derive session keys via HKDF.
 */
static int do_handshake(UdpSocket *sock, SessionKeys *keys)
{
    DhCtx  *ctx = dh_ctx_new();
    if (!ctx) return -1;

    uint8_t pubkey[512];
    size_t  pubkey_len = sizeof(pubkey);

    if (dh_generate_keypair(ctx, pubkey, &pubkey_len) != 0)
        goto fail;

    /* Send our DH public key wrapped in a HANDSHAKE packet */
    Packet hpkt;
    memset(&hpkt, 0, sizeof(hpkt));
    hpkt.type        = PKT_HANDSHAKE;
    hpkt.seq_num     = 0;
    hpkt.payload_len = (uint16_t)pubkey_len;
    memcpy(hpkt.payload, pubkey, pubkey_len);

    uint8_t buf[MAX_BUF_SIZE];
    size_t  len = packet_serialize(&hpkt, buf, sizeof(buf));
    if (sock_send(sock, buf, len) != 0) goto fail;

    /* Wait for peer's public key */
    int n = sock_recv(sock, buf, sizeof(buf), 5000);
    if (n <= 0) goto fail;

    Packet rpkt;
    if (packet_deserialize(&rpkt, buf, (size_t)n) != 0) goto fail;
    if (rpkt.type != PKT_HANDSHAKE) goto fail;

    /* Compute shared secret and derive session keys */
    uint8_t shared[512];
    size_t  shared_len = sizeof(shared);
    if (dh_compute_shared(ctx, rpkt.payload, rpkt.payload_len,
                          shared, &shared_len) != 0)
        goto fail;

    if (hkdf_derive_session_keys(shared, shared_len, keys) != 0)
        goto fail;

    dh_ctx_free(ctx);
    return 0;
fail:
    dh_ctx_free(ctx);
    return -1;
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <dest_ip> <port> <file>\n", argv[0]);
        return 1;
    }

    const char *dest_ip   = argv[1];
    uint16_t    port      = (uint16_t)atoi(argv[2]);
    const char *filepath  = argv[3];

    FILE *fp = fopen(filepath, "rb");
    if (!fp) { perror("fopen"); return 1; }

    UdpSocket sock;
    if (sock_create_sender(&sock, dest_ip, port) != 0) return 1;

    printf("[sender] connecting to %s:%u\n", dest_ip, port);

    SessionKeys keys;
    if (do_handshake(&sock, &keys) != 0) {
        fprintf(stderr, "[sender] handshake failed\n");
        return 1;
    }
    printf("[sender] handshake complete, session keys established\n");

    SenderARQ arq;
    arq_sender_init(&arq);

    uint8_t  plaintext[MAX_PAYLOAD_SIZE];
    uint8_t  ack_buf[MAX_BUF_SIZE];
    uint32_t seq = 0;
    size_t   n;

    while (!feof(fp) || arq.base < arq.next_seq) {

        /* Fill window as long as space and data available */
        while (!arq_sender_window_full(&arq) && !feof(fp)) {
            n = fread(plaintext, 1, MAX_PAYLOAD_SIZE, fp);
            if (n == 0) break;

            Packet pkt;
            pkt.seq_num = seq++;
            if (build_data_packet(&pkt, pkt.seq_num, plaintext, n, &keys) != 0) {
                fprintf(stderr, "[sender] packet build failed at seq %u\n", pkt.seq_num);
                break;
            }

            if (arq_sender_add(&arq, &pkt) < 0) break;
            if (send_packet(&sock, &pkt, &keys) != 0)
                fprintf(stderr, "[sender] send failed seq=%u\n", pkt.seq_num);
            else
                printf("[sender] sent seq=%u\n", pkt.seq_num);
        }

        /* Poll for ACKs with short timeout */
        int r = sock_recv(&sock, ack_buf, sizeof(ack_buf), arq.rto_ms / 4);
        if (r > 0) {
            Packet ack;
            if (packet_deserialize(&ack, ack_buf, (size_t)r) == 0) {
                if (ack.type == PKT_ACK) {
                    arq_sender_ack(&arq, ack.seq_num);
                    printf("[sender] acked seq=%u  cwnd=%u\n", ack.seq_num, arq.cwnd);
                } else if (ack.type == PKT_SACK) {
                    uint64_t bitmap;
                    memcpy(&bitmap, ack.payload, sizeof(bitmap));
                    arq_sender_sack(&arq, ack.seq_num, bitmap);
                }
            }
        }

        /* Retransmit timed-out packets */
        uint32_t timed_out[WINDOW_SIZE];
        int count = arq_sender_get_timed_out(&arq, 0, timed_out, WINDOW_SIZE);
        for (int i = 0; i < count; i++) {
            SendSlot *s = &arq.window[timed_out[i] % WINDOW_SIZE];
            printf("[sender] retransmit seq=%u (retry %d)\n",
                   timed_out[i], s->retries);
            send_packet(&sock, &s->pkt, &keys);
            if (s->retries == 1)
                cc_on_timeout(&arq);
        }
    }

    /* Send FIN */
    Packet fin;
    memset(&fin, 0, sizeof(fin));
    fin.type    = PKT_FIN;
    fin.seq_num = seq;
    send_packet(&sock, &fin, &keys);
    printf("[sender] FIN sent, transfer complete\n");

    fclose(fp);
    sock_close(&sock);
    return 0;
}
