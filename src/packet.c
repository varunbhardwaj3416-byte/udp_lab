#include <string.h>
#include <arpa/inet.h>
#include "packet.h"
#include "crypto.h"

/*
 * Serialise a Packet into a flat byte buffer for transmission.
 * All multi-byte integers written in network byte order.
 * Returns total bytes written, or 0 on error.
 */
size_t packet_serialize(const Packet *pkt, uint8_t *buf, size_t buf_len)
{
    size_t total = SEQ_NUM_SIZE + CRC32_SIZE + 1
                   + GCM_NONCE_LEN + GCM_TAG_LEN
                   + 2 + pkt->payload_len + HMAC_LEN;

    if (buf_len < total)
        return 0;

    uint8_t *p = buf;

    uint32_t seq_be = htonl(pkt->seq_num);
    memcpy(p, &seq_be, 4);          p += 4;

    uint32_t crc_be = htonl(pkt->crc32);
    memcpy(p, &crc_be, 4);          p += 4;

    *p++ = pkt->type;

    memcpy(p, pkt->nonce, GCM_NONCE_LEN);    p += GCM_NONCE_LEN;
    memcpy(p, pkt->gcm_tag, GCM_TAG_LEN);    p += GCM_TAG_LEN;

    uint16_t plen_be = htons(pkt->payload_len);
    memcpy(p, &plen_be, 2);         p += 2;

    memcpy(p, pkt->payload, pkt->payload_len); p += pkt->payload_len;

    memcpy(p, pkt->hmac, HMAC_LEN); p += HMAC_LEN;

    return (size_t)(p - buf);
}

/*
 * Deserialise a raw buffer into a Packet.
 * Returns 0 on success, -1 if buf_len is too small or malformed.
 */
int packet_deserialize(Packet *pkt, const uint8_t *buf, size_t buf_len)
{
    /* Minimum fixed-header size before variable payload */
    size_t min_size = SEQ_NUM_SIZE + CRC32_SIZE + 1
                      + GCM_NONCE_LEN + GCM_TAG_LEN + 2 + HMAC_LEN;
    if (buf_len < min_size)
        return -1;

    const uint8_t *p = buf;

    memcpy(&pkt->seq_num, p, 4);  pkt->seq_num = ntohl(pkt->seq_num);  p += 4;
    memcpy(&pkt->crc32,   p, 4);  pkt->crc32   = ntohl(pkt->crc32);    p += 4;

    pkt->type = *p++;

    memcpy(pkt->nonce,   p, GCM_NONCE_LEN);  p += GCM_NONCE_LEN;
    memcpy(pkt->gcm_tag, p, GCM_TAG_LEN);    p += GCM_TAG_LEN;

    memcpy(&pkt->payload_len, p, 2);
    pkt->payload_len = ntohs(pkt->payload_len); p += 2;

    if (pkt->payload_len > MAX_PAYLOAD_SIZE)
        return -1;
    if (buf_len < min_size + pkt->payload_len)
        return -1;

    memcpy(pkt->payload, p, pkt->payload_len); p += pkt->payload_len;
    memcpy(pkt->hmac,    p, HMAC_LEN);

    return 0;
}
