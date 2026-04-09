#include <string.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/dh.h>
#include <openssl/bn.h>
#include <openssl/kdf.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include "crypto.h"

/* ------------------------------------------------------------------ */
/* AES-256-GCM                                                         */
/* ------------------------------------------------------------------ */

int crypto_aes_gcm_encrypt(const uint8_t *key, const uint8_t *nonce,
                            const uint8_t *plaintext, size_t pt_len,
                            uint8_t *ciphertext, uint8_t *tag)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;

    int ret = -1, len = 0;

    if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
        goto cleanup;
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_NONCE_LEN, NULL))
        goto cleanup;
    if (!EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce))
        goto cleanup;
    if (!EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, (int)pt_len))
        goto cleanup;
    if (!EVP_EncryptFinal_ex(ctx, ciphertext + len, &len))
        goto cleanup;
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_LEN, tag))
        goto cleanup;

    ret = 0;
cleanup:
    EVP_CIPHER_CTX_free(ctx);
    return ret;
}

int crypto_aes_gcm_decrypt(const uint8_t *key, const uint8_t *nonce,
                            const uint8_t *ciphertext, size_t ct_len,
                            const uint8_t *tag,
                            uint8_t *plaintext)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;

    int ret = -1, len = 0;

    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
        goto cleanup;
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_NONCE_LEN, NULL))
        goto cleanup;
    if (!EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce))
        goto cleanup;
    if (!EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, (int)ct_len))
        goto cleanup;
    /* Set expected GCM tag before finalising */
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_LEN, (void *)tag))
        goto cleanup;
    /* EVP_DecryptFinal_ex returns 0 if tag verification fails */
    if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) <= 0)
        goto cleanup;

    ret = 0;
cleanup:
    EVP_CIPHER_CTX_free(ctx);
    return ret;
}

/* ------------------------------------------------------------------ */
/* HMAC-SHA256                                                         */
/* ------------------------------------------------------------------ */

int crypto_hmac_sha256(const uint8_t *key, size_t key_len,
                       const uint8_t *data, size_t data_len,
                       uint8_t *out)
{
    unsigned int out_len = HMAC_LEN;
    if (!HMAC(EVP_sha256(), key, (int)key_len, data, data_len, out, &out_len))
        return -1;
    return 0;
}

/* Constant-time comparison — prevents timing side-channel */
int crypto_memcmp(const uint8_t *a, const uint8_t *b, size_t len)
{
    return CRYPTO_memcmp(a, b, len);
}

/* ------------------------------------------------------------------ */
/* CRC32 (IEEE 802.3 polynomial 0xEDB88320)                           */
/* ------------------------------------------------------------------ */

static uint32_t crc32_table[256];
static int      crc32_table_ready = 0;

static void crc32_init_table(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        crc32_table[i] = c;
    }
    crc32_table_ready = 1;
}

uint32_t crc32_compute(const uint8_t *data, size_t len)
{
    if (!crc32_table_ready)
        crc32_init_table();

    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++)
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

/* ------------------------------------------------------------------ */
/* Diffie-Hellman (RFC 3526 Group 14, 2048-bit MODP)                  */
/* ------------------------------------------------------------------ */

struct DhCtx {
    EVP_PKEY     *pkey;      /* our DH key pair */
    EVP_PKEY_CTX *pctx;
};

/* RFC 3526 Group 14 2048-bit prime (hex) */
static const char DH_PRIME_HEX[] =
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
    "29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
    "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
    "C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
    "83655D23DCA3AD961C62F356208552BB9ED529077096966D"
    "670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
    "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9"
    "DE2BCBF6955817183995497CEA956AE515D2261898FA0510"
    "15728E5A8AACAA68FFFFFFFFFFFFFFFF";

DhCtx *dh_ctx_new(void)
{
    DhCtx *ctx = calloc(1, sizeof(DhCtx));
    if (!ctx) return NULL;

    BIGNUM *p = NULL, *g = NULL;
    DH     *dh = NULL;

    BN_hex2bn(&p, DH_PRIME_HEX);
    g = BN_new();
    BN_set_word(g, 2);   /* generator = 2 for Group 14 */

    dh = DH_new();
    DH_set0_pqg(dh, p, NULL, g);

    ctx->pkey = EVP_PKEY_new();
    EVP_PKEY_set1_DH(ctx->pkey, dh);
    DH_free(dh);

    return ctx;
}

void dh_ctx_free(DhCtx *ctx)
{
    if (!ctx) return;
    EVP_PKEY_free(ctx->pkey);
    EVP_PKEY_CTX_free(ctx->pctx);
    free(ctx);
}

int dh_generate_keypair(DhCtx *ctx, uint8_t *pubkey_buf, size_t *pubkey_len)
{
    ctx->pctx = EVP_PKEY_CTX_new(ctx->pkey, NULL);
    if (!ctx->pctx) return -1;

    EVP_PKEY *params = ctx->pkey;
    EVP_PKEY_CTX *kctx = EVP_PKEY_CTX_new(params, NULL);
    if (!kctx) return -1;

    EVP_PKEY *keypair = NULL;
    if (EVP_PKEY_keygen_init(kctx) <= 0) { EVP_PKEY_CTX_free(kctx); return -1; }
    if (EVP_PKEY_keygen(kctx, &keypair) <= 0) { EVP_PKEY_CTX_free(kctx); return -1; }
    EVP_PKEY_CTX_free(kctx);

    EVP_PKEY_free(ctx->pkey);
    ctx->pkey = keypair;

    /* Serialise public key as DER */
    int len = i2d_PublicKey(ctx->pkey, &pubkey_buf);
    if (len < 0) return -1;
    *pubkey_len = (size_t)len;
    return 0;
}

int dh_compute_shared(DhCtx *ctx,
                      const uint8_t *peer_pubkey, size_t peer_pubkey_len,
                      uint8_t *shared_secret, size_t *shared_len)
{
    /* Deserialise peer's public key */
    const uint8_t *pp = peer_pubkey;
    EVP_PKEY *peer = d2i_PublicKey(EVP_PKEY_DH, NULL, &pp, (long)peer_pubkey_len);
    if (!peer) return -1;

    EVP_PKEY_CTX *dctx = EVP_PKEY_CTX_new(ctx->pkey, NULL);
    if (!dctx) { EVP_PKEY_free(peer); return -1; }

    int ret = -1;
    if (EVP_PKEY_derive_init(dctx) <= 0) goto cleanup;
    if (EVP_PKEY_derive_set_peer(dctx, peer) <= 0) goto cleanup;
    if (EVP_PKEY_derive(dctx, shared_secret, shared_len) <= 0) goto cleanup;
    ret = 0;

cleanup:
    EVP_PKEY_CTX_free(dctx);
    EVP_PKEY_free(peer);
    return ret;
}

/* ------------------------------------------------------------------ */
/* HKDF (RFC 5869, SHA-256)                                            */
/* Derives two independent 32-byte keys from the DH shared secret.    */
/* ------------------------------------------------------------------ */

int hkdf_derive_session_keys(const uint8_t *shared_secret, size_t secret_len,
                              SessionKeys *keys)
{
    EVP_KDF     *kdf = EVP_KDF_fetch(NULL, "HKDF", NULL);
    EVP_KDF_CTX *kctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);
    if (!kctx) return -1;

    /* info strings enforce key separation */
    static const uint8_t info_enc[]  = "udp-file-transfer-enc-key-v1";
    static const uint8_t info_mac[]  = "udp-file-transfer-mac-key-v1";
    static const uint8_t salt[]      = "udp-file-transfer-salt-v1";

    OSSL_PARAM params[5];
    int ret = -1;

    /* Derive encryption key */
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, "SHA256", 0);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, (void *)shared_secret, secret_len);
    params[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, (void *)salt, sizeof(salt) - 1);
    params[3] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, (void *)info_enc, sizeof(info_enc) - 1);
    params[4] = OSSL_PARAM_construct_end();

    if (EVP_KDF_derive(kctx, keys->enc_key, AES_KEY_LEN, params) <= 0)
        goto cleanup;

    /* Reset context and derive MAC key with different info */
    EVP_KDF_CTX_reset(kctx);
    params[3] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, (void *)info_mac, sizeof(info_mac) - 1);

    if (EVP_KDF_derive(kctx, keys->mac_key, HMAC_KEY_LEN, params) <= 0)
        goto cleanup;

    ret = 0;
cleanup:
    EVP_KDF_CTX_free(kctx);
    return ret;
}

int crypto_random_bytes(uint8_t *buf, size_t len)
{
    return RAND_bytes(buf, (int)len) == 1 ? 0 : -1;
}
