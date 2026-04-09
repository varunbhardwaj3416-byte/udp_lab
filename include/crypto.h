#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <stddef.h>

#define AES_KEY_LEN   32   /* AES-256 */
#define HMAC_KEY_LEN  32   /* HMAC-SHA256 */
#define DH_PRIME_BITS 2048 /* RFC 3526 Group 14 */

/* Session keys derived via HKDF after DH handshake */
typedef struct {
    uint8_t enc_key[AES_KEY_LEN];
    uint8_t mac_key[HMAC_KEY_LEN];
} SessionKeys;

/* DH context — holds OpenSSL EVP_PKEY internally */
typedef struct DhCtx DhCtx;

/* --- AES-256-GCM --- */
int  crypto_aes_gcm_encrypt(const uint8_t *key, const uint8_t *nonce,
                             const uint8_t *plaintext, size_t pt_len,
                             uint8_t *ciphertext, uint8_t *tag);

int  crypto_aes_gcm_decrypt(const uint8_t *key, const uint8_t *nonce,
                             const uint8_t *ciphertext, size_t ct_len,
                             const uint8_t *tag,
                             uint8_t *plaintext);

/* --- HMAC-SHA256 --- */
int  crypto_hmac_sha256(const uint8_t *key, size_t key_len,
                        const uint8_t *data, size_t data_len,
                        uint8_t *out);      /* out must be >= 32 bytes */

/* Constant-time comparison to prevent timing attacks */
int  crypto_memcmp(const uint8_t *a, const uint8_t *b, size_t len);

/* --- CRC32 (IEEE 802.3) --- */
uint32_t crc32_compute(const uint8_t *data, size_t len);

/* --- Diffie-Hellman (RFC 3526 Group 14, 2048-bit) --- */
DhCtx   *dh_ctx_new(void);
void     dh_ctx_free(DhCtx *ctx);

/* Generate DH private/public key pair; write public key to buf */
int      dh_generate_keypair(DhCtx *ctx, uint8_t *pubkey_buf, size_t *pubkey_len);

/* Compute shared secret from peer's public key */
int      dh_compute_shared(DhCtx *ctx,
                            const uint8_t *peer_pubkey, size_t peer_pubkey_len,
                            uint8_t *shared_secret, size_t *shared_len);

/* --- HKDF (RFC 5869, SHA-256) --- */
/* Derives enc_key and mac_key from the raw DH shared secret */
int      hkdf_derive_session_keys(const uint8_t *shared_secret, size_t secret_len,
                                  SessionKeys *keys);

/* Fill buf with cryptographically random bytes (RAND_bytes wrapper) */
int      crypto_random_bytes(uint8_t *buf, size_t len);

#endif /* CRYPTO_H */
