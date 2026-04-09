#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <stddef.h>

#define MAX_PAYLOAD_SIZE   1400
#define GCM_TAG_LEN        16
#define GCM_NONCE_LEN      12
#define HMAC_LEN           32
#define SEQ_NUM_SIZE       4
#define CRC32_SIZE         4

/* Packet type flags */
typedef enum {
    PKT_DATA      = 0x01,
    PKT_ACK       = 0x02,
    PKT_SACK      = 0x03,
    PKT_FIN       = 0x04,
    PKT_FINACK    = 0x05,
    PKT_HANDSHAKE = 0x06,
    PKT_SESSION   = 0x07,
} PktType;

/*
 * Wire format (in order):
 *   [4]  seq_num
 *   [4]  crc32         -- over all fields except crc32 itself and hmac
 *   [1]  type
 *   [12] nonce         -- AES-GCM nonce, unique per packet
 *   [16] gcm_tag       -- AES-GCM authentication tag
 *   [N]  payload       -- encrypted ciphertext (N <= MAX_PAYLOAD_SIZE)
 *   [32] hmac          -- HMAC-SHA256 over all preceding bytes
 */
typedef struct {
    uint32_t seq_num;
    uint32_t crc32;
    uint8_t  type;
    uint8_t  nonce[GCM_NONCE_LEN];
    uint8_t  gcm_tag[GCM_TAG_LEN];
    uint16_t payload_len;
    uint8_t  payload[MAX_PAYLOAD_SIZE];
    uint8_t  hmac[HMAC_LEN];
} Packet;

/* Serialise/deserialise to/from raw bytes for sendto/recvfrom */
size_t   packet_serialize(const Packet *pkt, uint8_t *buf, size_t buf_len);
int      packet_deserialize(Packet *pkt, const uint8_t *buf, size_t buf_len);

#endif /* PACKET_H */
