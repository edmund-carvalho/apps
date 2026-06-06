/**
 * Copyright (c) 2026 Edmund C
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/**
 * @file ciphergen.h
 * @brief User-friendly wrapper around OpenSSL EVP symmetric ciphers.
 *
 * Offers two API levels:
 * - Low‑level: CipherCtx for streaming / custom workflows.
 * - High‑level: aes_xxx_encrypt / aes_xxx_decrypt for one‑shot operations.
 *
 * All functions return 0 on success, -1 on failure (and print
 * the OpenSSL error stack to stderr).
 */
#ifndef CIPHERGEN_H
#define CIPHERGEN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Supported AES modes. */
typedef enum {
    CIPHER_MODE_CBC = 0,
    CIPHER_MODE_CTR,
    CIPHER_MODE_GCM,
    CIPHER_MODE_XTS
} cipher_mode_t;

/** Opaque context for low‑level encryption/decryption operations. */
typedef struct cipher_ctx CipherCtx;

/* ====================================================================== */
/* Low‑level generic API (streaming, full control)                        */
/* ====================================================================== */

/**
 * @brief Allocate a new cipher context.
 * @return Pointer to context, or NULL on allocation failure.
 */
CipherCtx *cipher_ctx_new(void);

/**
 * @brief Free a cipher context.
 * @param ctx  Context to free (may be NULL).
 */
void cipher_ctx_free(CipherCtx *ctx);

/**
 * @brief Initialise the context for a specific operation.
 *
 * For GCM mode the IV length must be supplied (common values: 12, 16).
 * For all other modes @p iv_len is ignored (block size 16 is used internally).
 *
 * @param ctx       Context allocated by cipher_ctx_new().
 * @param key       Encryption/decryption key buffer.
 * @param key_len   Length of the key in bytes (16, 24, or 32 for AES-128/192/256;
 *                  for XTS the key is double‑length: 32 or 64 bytes).
 * @param iv        Initialisation vector (must be at least the block size,
 *                  or @p iv_len bytes for GCM).
 * @param iv_len    IV length in bytes (only used for GCM).
 * @param enc       1 for encryption, 0 for decryption.
 * @param mode      Cipher mode (CIPHER_MODE_CBC, …).
 * @param key_bits  AES key size in bits (128, 192, or 256).
 * @return 0 on success, -1 on error.
 */
int cipher_init(CipherCtx *ctx,
                const uint8_t *key, size_t key_len,
                const uint8_t *iv, size_t iv_len,
                int enc, cipher_mode_t mode, int key_bits);

/**
 * @brief Enable or disable PKCS#7 padding (CBC mode only).
 *
 * @param ctx     Initialised context.
 * @param padding 1 to enable, 0 to disable.
 * @return 0 on success, -1 on error.
 */
int cipher_set_padding(CipherCtx *ctx, int padding);

/**
 * @brief Feed plaintext or ciphertext data.
 *
 * @param ctx     Initialised context.
 * @param in      Input data buffer.
 * @param in_len  Number of input bytes.
 * @param out     Output buffer (must be at least in_len + block size).
 * @param out_len Number of bytes written to out.
 * @return 0 on success, -1 on error.
 */
int cipher_update(CipherCtx *ctx,
                  const uint8_t *in, size_t in_len,
                  uint8_t *out, size_t *out_len);

/**
 * @brief Feed Additional Authenticated Data (AAD) - GCM mode only.
 *
 * Must be called before any cipher_update() for the same context.
 *
 * @param ctx     Initialised context.
 * @param aad     AAD buffer.
 * @param aad_len Length of AAD in bytes.
 * @return 0 on success, -1 on error.
 */
int cipher_update_aad(CipherCtx *ctx,
                      const uint8_t *aad, size_t aad_len);

/**
 * @brief Finalise the operation.
 *
 * For GCM encryption this finalises and allows tag retrieval;
 * for GCM decryption this verifies the tag.
 *
 * @param ctx     Initialised context.
 * @param out     Output buffer for the final bytes.
 * @param out_len Number of bytes written.
 * @return 0 on success, -1 on error (for GCM decryption, -1 means tag mismatch).
 */
int cipher_final(CipherCtx *ctx, uint8_t *out, size_t *out_len);

/**
 * @brief Retrieve the GCM authentication tag after encryption.
 *
 * @param ctx      Initialised context (encryption, after cipher_final()).
 * @param tag      Buffer to receive the tag.
 * @param tag_len  Desired tag length (typically 12 or 16 bytes).
 * @return 0 on success, -1 on error.
 */
int cipher_get_tag(CipherCtx *ctx, uint8_t *tag, size_t tag_len);

/**
 * @brief Set the expected GCM authentication tag before decryption.
 *
 * @param ctx      Initialised context (decryption).
 * @param tag      Expected tag buffer.
 * @param tag_len  Length of the tag in bytes.
 * @return 0 on success, -1 on error.
 */
int cipher_set_tag(CipherCtx *ctx, const uint8_t *tag, size_t tag_len);

/* ====================================================================== */
/* High‑level convenience functions (one‑shot encrypt / decrypt)          */
/* ====================================================================== */

/**
 * @brief AES‑CBC encryption (single call).
 * @param plaintext    Input plaintext.
 * @param pt_len       Length of plaintext in bytes.
 * @param key          AES key (16, 24, or 32 bytes).
 * @param key_len      Key length in bytes.
 * @param iv           16‑byte initialisation vector.
 * @param padding      1 to enable PKCS#7 padding, 0 to disable.
 * @param ciphertext   Output buffer (must be at least pt_len + 16).
 * @param ct_len       Number of ciphertext bytes written.
 * @return 0 on success, -1 on error.
 */
int aes_cbc_encrypt(const uint8_t *plaintext, size_t pt_len,
                    const uint8_t *key, size_t key_len,
                    const uint8_t *iv, int padding,
                    uint8_t *ciphertext, size_t *ct_len);

/**
 * @brief AES‑CBC decryption (single call).
 * @param ciphertext   Input ciphertext.
 * @param ct_len       Length of ciphertext in bytes.
 * @param key          AES key.
 * @param key_len      Key length in bytes.
 * @param iv           16‑byte IV.
 * @param padding      1 if ciphertext was padded, 0 if not.
 * @param plaintext    Output buffer for decrypted data.
 * @param pt_len       Number of plaintext bytes written.
 * @return 0 on success, -1 on error.
 */
int aes_cbc_decrypt(const uint8_t *ciphertext, size_t ct_len,
                    const uint8_t *key, size_t key_len,
                    const uint8_t *iv, int padding,
                    uint8_t *plaintext, size_t *pt_len);

/**
 * @brief AES‑CTR encryption (CTR is identical for enc/dec, but kept for clarity).
 * @param plaintext    Input plaintext.
 * @param pt_len       Length of plaintext in bytes.
 * @param key          AES key.
 * @param key_len      Key length in bytes (16, 24, or 32).
 * @param iv           16‑byte counter block (nonce).
 * @param ciphertext   Output buffer (same length as plaintext).
 * @param ct_len       Number of ciphertext bytes written (= pt_len).
 * @return 0 on success, -1 on error.
 */
int aes_ctr_encrypt(const uint8_t *plaintext, size_t pt_len,
                    const uint8_t *key, size_t key_len,
                    const uint8_t *iv,
                    uint8_t *ciphertext, size_t *ct_len);

/**
 * @brief AES‑CTR decryption (same operation as encryption).
 */
int aes_ctr_decrypt(const uint8_t *ciphertext, size_t ct_len,
                    const uint8_t *key, size_t key_len,
                    const uint8_t *iv,
                    uint8_t *plaintext, size_t *pt_len);

/**
 * @brief AES‑GCM encryption (single call with AAD).
 * @param plaintext    Input plaintext.
 * @param pt_len       Length of plaintext in bytes.
 * @param aad          Additional authenticated data (may be NULL if aad_len == 0).
 * @param aad_len      Length of AAD in bytes.
 * @param key          AES key.
 * @param key_len      Key length in bytes (16, 24, or 32).
 * @param iv           Initialisation vector (common: 12 bytes).
 * @param iv_len       IV length in bytes.
 * @param ciphertext   Output buffer (at least pt_len + 16).
 * @param ct_len       Number of ciphertext bytes written.
 * @param tag          Output buffer for authentication tag.
 * @param tag_len      Desired tag length (typically 12 or 16).
 * @return 0 on success, -1 on error.
 */
int aes_gcm_encrypt(const uint8_t *plaintext, size_t pt_len,
                    const uint8_t *aad, size_t aad_len,
                    const uint8_t *key, size_t key_len,
                    const uint8_t *iv, size_t iv_len,
                    uint8_t *ciphertext, size_t *ct_len,
                    uint8_t *tag, size_t tag_len);

/**
 * @brief AES‑GCM decryption (single call with AAD and tag verification).
 * @param ciphertext   Input ciphertext.
 * @param ct_len       Length of ciphertext in bytes.
 * @param aad          Additional authenticated data (may be NULL).
 * @param aad_len      AAD length.
 * @param tag          Expected authentication tag.
 * @param tag_len      Tag length in bytes.
 * @param key          AES key.
 * @param key_len      Key length.
 * @param iv           Initialisation vector.
 * @param iv_len       IV length.
 * @param plaintext    Output buffer for decrypted data.
 * @param pt_len       Number of plaintext bytes written.
 * @return 0 on success, -1 on error (including tag mismatch).
 */
int aes_gcm_decrypt(const uint8_t *ciphertext, size_t ct_len,
                    const uint8_t *aad, size_t aad_len,
                    const uint8_t *tag, size_t tag_len,
                    const uint8_t *key, size_t key_len,
                    const uint8_t *iv, size_t iv_len,
                    uint8_t *plaintext, size_t *pt_len);

/**
 * @brief AES‑XTS encryption (single call).
 * @note XTS key is double length: 32 bytes for AES‑128‑XTS, 64 bytes for AES‑256‑XTS.
 * @param plaintext    Input plaintext (must be at least one block: 16 bytes).
 * @param pt_len       Length of plaintext.
 * @param key          XTS key (key_len == 32 → AES‑128‑XTS, 64 → AES‑256‑XTS).
 * @param key_len      Key length in bytes.
 * @param iv           16‑byte tweak.
 * @param ciphertext   Output buffer (same length as plaintext).
 * @param ct_len       Number of bytes written.
 * @return 0 on success, -1 on error.
 */
int aes_xts_encrypt(const uint8_t *plaintext, size_t pt_len,
                    const uint8_t *key, size_t key_len,
                    const uint8_t *iv,
                    uint8_t *ciphertext, size_t *ct_len);

/**
 * @brief AES‑XTS decryption (single call).
 */
int aes_xts_decrypt(const uint8_t *ciphertext, size_t ct_len,
                    const uint8_t *key, size_t key_len,
                    const uint8_t *iv,
                    uint8_t *plaintext, size_t *pt_len);

#ifdef __cplusplus
}
#endif

#endif /* CIPHERGEN_H */