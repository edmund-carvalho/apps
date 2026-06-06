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
 * @file ciphergen.c
 * @brief Implementation of the OpenSSL cipher wrapper.
 */
#include "ciphergen.h"
#include <openssl/evp.h>
#include <openssl/err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Internal context structure. */
struct cipher_ctx {
    EVP_CIPHER_CTX *evp;   /**< OpenSSL cipher context. */
    cipher_mode_t mode;     /**< Selected cipher mode. */
    int key_bits;           /**< AES key size in bits. */
    int is_encrypt;         /**< 1 = encrypt, 0 = decrypt. */
    int padding;            /**< Padding flag (used for CBC). */
};

/** Print OpenSSL error stack to stderr. */
static void print_openssl_errors(void) {
    ERR_print_errors_fp(stderr);
}

/**
 * @brief Map our mode and key size to an EVP cipher object.
 *
 * @param mode     Cipher mode.
 * @param key_bits AES key size in bits.
 * @return EVP cipher, or NULL if unsupported.
 */
static const EVP_CIPHER *get_cipher(cipher_mode_t mode, int key_bits) {
    switch (mode) {
    case CIPHER_MODE_CBC:
        if (key_bits == 128) return EVP_aes_128_cbc();
        if (key_bits == 192) return EVP_aes_192_cbc();
        if (key_bits == 256) return EVP_aes_256_cbc();
        break;
    case CIPHER_MODE_CTR:
        if (key_bits == 128) return EVP_aes_128_ctr();
        if (key_bits == 192) return EVP_aes_192_ctr();
        if (key_bits == 256) return EVP_aes_256_ctr();
        break;
    case CIPHER_MODE_GCM:
        if (key_bits == 128) return EVP_aes_128_gcm();
        if (key_bits == 192) return EVP_aes_192_gcm();
        if (key_bits == 256) return EVP_aes_256_gcm();
        break;
    case CIPHER_MODE_XTS:
        if (key_bits == 128) return EVP_aes_128_xts();
        if (key_bits == 256) return EVP_aes_256_xts();
        break;
    default:
        return NULL;
    }
    return NULL;
}

/* -------------------------------------------------------------------------- */

CipherCtx *cipher_ctx_new(void) {
    CipherCtx *ctx = malloc(sizeof(*ctx));
    if (!ctx) return NULL;
    ctx->evp = EVP_CIPHER_CTX_new();
    if (!ctx->evp) {
        free(ctx);
        return NULL;
    }
    ctx->mode = CIPHER_MODE_CBC;
    ctx->key_bits = 128;
    ctx->is_encrypt = 1;
    ctx->padding = 1;
    return ctx;
}

void cipher_ctx_free(CipherCtx *ctx) {
    if (ctx) {
        EVP_CIPHER_CTX_free(ctx->evp);
        free(ctx);
    }
}

int cipher_init(CipherCtx *ctx,
                const uint8_t *key, size_t key_len,
                const uint8_t *iv, size_t iv_len,
                int enc, cipher_mode_t mode, int key_bits)
{
    const EVP_CIPHER *cipher = get_cipher(mode, key_bits);
    if (!cipher) {
        fprintf(stderr, "cipher_init: unsupported mode/key size\n");
        return -1;
    }

    /* Verify key length matches the cipher's expectation */
    if (key_len != (size_t)EVP_CIPHER_key_length(cipher)) {
        fprintf(stderr, "cipher_init: key length %zu, expected %d\n",
                key_len, EVP_CIPHER_key_length(cipher));
        return -1;
    }

    /* Step 1: Init with cipher only (no key/IV) */
    if (enc) {
        if (!EVP_EncryptInit_ex(ctx->evp, cipher, NULL, NULL, NULL)) {
            print_openssl_errors();
            return -1;
        }
    } else {
        if (!EVP_DecryptInit_ex(ctx->evp, cipher, NULL, NULL, NULL)) {
            print_openssl_errors();
            return -1;
        }
    }

    /* Step 2: For GCM, set IV length now (after cipher is set) */
    if (mode == CIPHER_MODE_GCM) {
        if (!EVP_CIPHER_CTX_ctrl(ctx->evp, EVP_CTRL_GCM_SET_IVLEN,
                                 (int)iv_len, NULL)) {
            print_openssl_errors();
            return -1;
        }
    }

    /* Step 3: Supply key and IV */
    if (enc) {
        if (!EVP_EncryptInit_ex(ctx->evp, NULL, NULL, key, iv)) {
            print_openssl_errors();
            return -1;
        }
    } else {
        if (!EVP_DecryptInit_ex(ctx->evp, NULL, NULL, key, iv)) {
            print_openssl_errors();
            return -1;
        }
    }

    ctx->mode = mode;
    ctx->key_bits = key_bits;
    ctx->is_encrypt = enc;
    return 0;
}

int cipher_set_padding(CipherCtx *ctx, int padding) {
    if (!EVP_CIPHER_CTX_set_padding(ctx->evp, padding ? 1 : 0)) {
        print_openssl_errors();
        return -1;
    }
    ctx->padding = padding;
    return 0;
}

int cipher_update(CipherCtx *ctx,
                  const uint8_t *in, size_t in_len,
                  uint8_t *out, size_t *out_len)
{
    int len = 0;
    if (ctx->is_encrypt) {
        if (!EVP_EncryptUpdate(ctx->evp, out, &len, in, (int)in_len)) {
            print_openssl_errors();
            return -1;
        }
    } else {
        if (!EVP_DecryptUpdate(ctx->evp, out, &len, in, (int)in_len)) {
            print_openssl_errors();
            return -1;
        }
    }
    *out_len = len;
    return 0;
}

int cipher_update_aad(CipherCtx *ctx,
                      const uint8_t *aad, size_t aad_len)
{
    int len = 0;
    /* GCM AAD: call Update with NULL output */
    if (!EVP_DecryptUpdate(ctx->evp, NULL, &len, aad, (int)aad_len) &&
        !EVP_EncryptUpdate(ctx->evp, NULL, &len, aad, (int)aad_len)) {
        print_openssl_errors();
        return -1;
    }
    return 0;
}

int cipher_final(CipherCtx *ctx, uint8_t *out, size_t *out_len) {
    int len = 0;
    if (ctx->is_encrypt) {
        if (!EVP_EncryptFinal_ex(ctx->evp, out, &len)) {
            print_openssl_errors();
            return -1;
        }
    } else {
        if (!EVP_DecryptFinal_ex(ctx->evp, out, &len)) {
            print_openssl_errors();
            return -1;
        }
    }
    *out_len = len;
    return 0;
}

int cipher_get_tag(CipherCtx *ctx, uint8_t *tag, size_t tag_len) {
    if (!EVP_CIPHER_CTX_ctrl(ctx->evp, EVP_CTRL_GCM_GET_TAG,
                             (int)tag_len, tag)) {
        print_openssl_errors();
        return -1;
    }
    return 0;
}

int cipher_set_tag(CipherCtx *ctx, const uint8_t *tag, size_t tag_len) {
    if (!EVP_CIPHER_CTX_ctrl(ctx->evp, EVP_CTRL_GCM_SET_TAG,
                             (int)tag_len, (void*)tag)) {
        print_openssl_errors();
        return -1;
    }
    return 0;
}



/* ====================================================================== */
/* High-level convenience functions                                       */
/* ====================================================================== */

int aes_cbc_encrypt(const uint8_t *plaintext, size_t pt_len,
                    const uint8_t *key, size_t key_len,
                    const uint8_t *iv, int padding,
                    uint8_t *ciphertext, size_t *ct_len)
{
    CipherCtx *ctx = cipher_ctx_new();
    if (!ctx) return -1;

    int key_bits = (int)key_len * 8;
    if (cipher_init(ctx, key, key_len, iv, 16, 1, CIPHER_MODE_CBC, key_bits)) {
        cipher_ctx_free(ctx);
        return -1;
    }
    if (cipher_set_padding(ctx, padding)) {
        cipher_ctx_free(ctx);
        return -1;
    }

    size_t out_len = 0, final_len = 0;
    if (cipher_update(ctx, plaintext, pt_len, ciphertext, &out_len)) {
        cipher_ctx_free(ctx);
        return -1;
    }
    if (cipher_final(ctx, ciphertext + out_len, &final_len)) {
        cipher_ctx_free(ctx);
        return -1;
    }
    *ct_len = out_len + final_len;
    cipher_ctx_free(ctx);
    return 0;
}

int aes_cbc_decrypt(const uint8_t *ciphertext, size_t ct_len,
                    const uint8_t *key, size_t key_len,
                    const uint8_t *iv, int padding,
                    uint8_t *plaintext, size_t *pt_len)
{
    CipherCtx *ctx = cipher_ctx_new();
    if (!ctx) return -1;

    int key_bits = (int)key_len * 8;
    if (cipher_init(ctx, key, key_len, iv, 16, 0, CIPHER_MODE_CBC, key_bits)) {
        cipher_ctx_free(ctx);
        return -1;
    }
    if (cipher_set_padding(ctx, padding)) {
        cipher_ctx_free(ctx);
        return -1;
    }

    size_t out_len = 0, final_len = 0;
    if (cipher_update(ctx, ciphertext, ct_len, plaintext, &out_len)) {
        cipher_ctx_free(ctx);
        return -1;
    }
    if (cipher_final(ctx, plaintext + out_len, &final_len)) {
        cipher_ctx_free(ctx);
        return -1;
    }
    *pt_len = out_len + final_len;
    cipher_ctx_free(ctx);
    return 0;
}

int aes_ctr_encrypt(const uint8_t *plaintext, size_t pt_len,
                    const uint8_t *key, size_t key_len,
                    const uint8_t *iv,
                    uint8_t *ciphertext, size_t *ct_len)
{
    CipherCtx *ctx = cipher_ctx_new();
    if (!ctx) return -1;

    int key_bits = (int)key_len * 8;
    if (cipher_init(ctx, key, key_len, iv, 16, 1, CIPHER_MODE_CTR, key_bits)) {
        cipher_ctx_free(ctx);
        return -1;
    }

    size_t out_len = 0, final_len = 0;
    if (cipher_update(ctx, plaintext, pt_len, ciphertext, &out_len)) {
        cipher_ctx_free(ctx);
        return -1;
    }
    if (cipher_final(ctx, ciphertext + out_len, &final_len)) {
        cipher_ctx_free(ctx);
        return -1;
    }
    *ct_len = out_len + final_len;
    cipher_ctx_free(ctx);
    return 0;
}

int aes_ctr_decrypt(const uint8_t *ciphertext, size_t ct_len,
                    const uint8_t *key, size_t key_len,
                    const uint8_t *iv,
                    uint8_t *plaintext, size_t *pt_len)
{
    /* CTR decrypt is identical to encrypt */
    return aes_ctr_encrypt(ciphertext, ct_len, key, key_len, iv, plaintext, pt_len);
}

int aes_gcm_encrypt(const uint8_t *plaintext, size_t pt_len,
                    const uint8_t *aad, size_t aad_len,
                    const uint8_t *key, size_t key_len,
                    const uint8_t *iv, size_t iv_len,
                    uint8_t *ciphertext, size_t *ct_len,
                    uint8_t *tag, size_t tag_len)
{
    CipherCtx *ctx = cipher_ctx_new();
    if (!ctx) return -1;

    int key_bits = (int)key_len * 8;
    if (cipher_init(ctx, key, key_len, iv, iv_len, 1, CIPHER_MODE_GCM, key_bits)) {
        cipher_ctx_free(ctx);
        return -1;
    }

    /* Feed AAD if present */
    if (aad && aad_len > 0) {
        if (cipher_update_aad(ctx, aad, aad_len)) {
            cipher_ctx_free(ctx);
            return -1;
        }
    }

    size_t out_len = 0, final_len = 0;
    if (cipher_update(ctx, plaintext, pt_len, ciphertext, &out_len)) {
        cipher_ctx_free(ctx);
        return -1;
    }
    if (cipher_final(ctx, ciphertext + out_len, &final_len)) {
        cipher_ctx_free(ctx);
        return -1;
    }
    *ct_len = out_len + final_len;

    if (cipher_get_tag(ctx, tag, tag_len)) {
        cipher_ctx_free(ctx);
        return -1;
    }

    cipher_ctx_free(ctx);
    return 0;
}

int aes_gcm_decrypt(const uint8_t *ciphertext, size_t ct_len,
                    const uint8_t *aad, size_t aad_len,
                    const uint8_t *tag, size_t tag_len,
                    const uint8_t *key, size_t key_len,
                    const uint8_t *iv, size_t iv_len,
                    uint8_t *plaintext, size_t *pt_len)
{
    CipherCtx *ctx = cipher_ctx_new();
    if (!ctx) return -1;

    int key_bits = (int)key_len * 8;
    if (cipher_init(ctx, key, key_len, iv, iv_len, 0, CIPHER_MODE_GCM, key_bits)) {
        cipher_ctx_free(ctx);
        return -1;
    }

    if (cipher_set_tag(ctx, tag, tag_len)) {
        cipher_ctx_free(ctx);
        return -1;
    }

    if (aad && aad_len > 0) {
        if (cipher_update_aad(ctx, aad, aad_len)) {
            cipher_ctx_free(ctx);
            return -1;
        }
    }

    size_t out_len = 0, final_len = 0;
    if (cipher_update(ctx, ciphertext, ct_len, plaintext, &out_len)) {
        cipher_ctx_free(ctx);
        return -1;
    }
    if (cipher_final(ctx, plaintext + out_len, &final_len)) {
        cipher_ctx_free(ctx);
        return -1;
    }
    *pt_len = out_len + final_len;

    cipher_ctx_free(ctx);
    return 0;
}

int aes_xts_encrypt(const uint8_t *plaintext, size_t pt_len,
                    const uint8_t *key, size_t key_len,
                    const uint8_t *iv,
                    uint8_t *ciphertext, size_t *ct_len)
{
    CipherCtx *ctx = cipher_ctx_new();
    if (!ctx) return -1;

    /* XTS key bits: 128 for 32-byte key, 256 for 64-byte key */
    int key_bits = (key_len == 32) ? 128 : 256;
    if (cipher_init(ctx, key, key_len, iv, 16, 1, CIPHER_MODE_XTS, key_bits)) {
        cipher_ctx_free(ctx);
        return -1;
    }

    size_t out_len = 0, final_len = 0;
    if (cipher_update(ctx, plaintext, pt_len, ciphertext, &out_len)) {
        cipher_ctx_free(ctx);
        return -1;
    }
    if (cipher_final(ctx, ciphertext + out_len, &final_len)) {
        cipher_ctx_free(ctx);
        return -1;
    }
    *ct_len = out_len + final_len;
    cipher_ctx_free(ctx);
    return 0;
}

int aes_xts_decrypt(const uint8_t *ciphertext, size_t ct_len,
                    const uint8_t *key, size_t key_len,
                    const uint8_t *iv,
                    uint8_t *plaintext, size_t *pt_len)
{
    CipherCtx *ctx = cipher_ctx_new();
    if (!ctx) return -1;

    int key_bits = (key_len == 32) ? 128 : 256;
    if (cipher_init(ctx, key, key_len, iv, 16, 0, CIPHER_MODE_XTS, key_bits)) {
        cipher_ctx_free(ctx);
        return -1;
    }

    size_t out_len = 0, final_len = 0;
    if (cipher_update(ctx, ciphertext, ct_len, plaintext, &out_len)) {
        cipher_ctx_free(ctx);
        return -1;
    }
    if (cipher_final(ctx, plaintext + out_len, &final_len)) {
        cipher_ctx_free(ctx);
        return -1;
    }
    *pt_len = out_len + final_len;
    cipher_ctx_free(ctx);
    return 0;
}