/**
 * @file testapp.c
 * @brief Hard-coded test harness using the high-level convenience API.
 */
#include "ciphergen.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* -------------------------------------------------------------------------- */
/* Utility functions                                                          */
/* -------------------------------------------------------------------------- */

static int hex_to_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hex_to_bytes(const char *hex, uint8_t *out, size_t out_max) {
    size_t len = 0;
    int high = -1;
    for (; *hex; hex++) {
        if (isspace((unsigned char)*hex)) continue;
        int val = hex_to_nibble(*hex);
        if (val < 0) return -1;
        if (high < 0) {
            high = val;
        } else {
            if (len >= out_max) return -1;
            out[len++] = (uint8_t)((high << 4) | val);
            high = -1;
        }
    }
    return (high < 0) ? (int)len : -1;
}

/* -------------------------------------------------------------------------- */
/* Test vector definition                                                     */
/* -------------------------------------------------------------------------- */

typedef struct {
    const char *name;
    cipher_mode_t mode;         /* not strictly needed by test logic, kept for info */
    int key_bits;               /* not needed for convenience functions */
    const char *key_hex;
    const char *iv_hex;
    size_t iv_len;              /* only for GCM */
    const char *pt_hex;
    const char *ct_hex;         /* NULL if round-trip only */
    const char *aad_hex;
    const char *tag_hex;
    int padding;                /* -1 = default, 0/1 for CBC */
} test_vector_t;

static const test_vector_t tests[] = {
    { "CBC-128 (no pad)", CIPHER_MODE_CBC, 128,
      "2b7e151628aed2a6abf7158809cf4f3c",
      "000102030405060708090a0b0c0d0e0f", 0,
      "6bc1bee22e409f96e93d7e117393172a",
      "7649abac8119b246cee98e9b12e9197d",
      NULL, NULL, 0 },

    { "CTR-256", CIPHER_MODE_CTR, 256,
      "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4",
      "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff", 0,
      "6bc1bee22e409f96e93d7e117393172a",
      "601ec313775789a5b7a7f504bbf3d228",
      NULL, NULL, -1 },

    { "GCM-128", CIPHER_MODE_GCM, 128,
      "c939cc13397c1d37de6ae0e1cb7c423c",
      "b3d8cc017cbb89b39e0f67e2", 12,
      "c3b3c41f113a31b73d9a5cd432103069",
      NULL,               /* round-trip + tag verification */
      "24825602bd12a984e0092d3e448eda5f",
      "0032a1dc85f1c9786925a2e71d8272dd",
      -1 },

    { "XTS-128", CIPHER_MODE_XTS, 128,
      "a1b90cba3f06ac353b2c343876081762090923026e91771815f29dab01932f2f",
      "4faef7117cda59c66e4b92013e768ad5", 0,
      "ebabce95b14d3c8d6fb350390790311c",
      NULL,               /* round-trip only */
      NULL, NULL, -1 },

    { NULL, 0, 0, NULL, NULL, 0, NULL, NULL, NULL, NULL, -1 }
};

/* -------------------------------------------------------------------------- */
/* Test runner                                                                */
/* -------------------------------------------------------------------------- */

static int run_single_test(const test_vector_t *tv) {
    uint8_t key[64], iv[32], pt[128], ct[128], tag[16], exp_tag[16], aad[64];
    size_t key_len, iv_len, pt_len, ct_len = 0, aad_len = 0, tag_len = 0;

    key_len = hex_to_bytes(tv->key_hex, key, sizeof(key));
    iv_len  = hex_to_bytes(tv->iv_hex, iv, sizeof(iv));
    pt_len  = hex_to_bytes(tv->pt_hex, pt, sizeof(pt));
    if (tv->ct_hex) {
        ct_len = hex_to_bytes(tv->ct_hex, ct, sizeof(ct));
    }
    if (tv->aad_hex) {
        aad_len = hex_to_bytes(tv->aad_hex, aad, sizeof(aad));
    }
    if (tv->tag_hex) {
        tag_len = hex_to_bytes(tv->tag_hex, exp_tag, sizeof(exp_tag));
    }

    /* ---- Encryption ---- */
    uint8_t out_buf[256];
    size_t out_len = 0;
    int ret = 0;

    switch (tv->mode) {
    case CIPHER_MODE_CBC:
        ret = aes_cbc_encrypt(pt, pt_len, key, key_len, iv,
                              tv->padding, out_buf, &out_len);
        break;
    case CIPHER_MODE_CTR:
        ret = aes_ctr_encrypt(pt, pt_len, key, key_len, iv,
                              out_buf, &out_len);
        break;
    case CIPHER_MODE_GCM:
        ret = aes_gcm_encrypt(pt, pt_len, (aad_len ? aad : NULL), aad_len,
                              key, key_len, iv, tv->iv_len,
                              out_buf, &out_len, tag, tag_len);
        break;
    case CIPHER_MODE_XTS:
        ret = aes_xts_encrypt(pt, pt_len, key, key_len, iv,
                              out_buf, &out_len);
        break;
    default:
        fprintf(stderr, "  [FAIL] %s: unknown mode\n", tv->name);
        return 1;
    }

    if (ret) {
        fprintf(stderr, "  [FAIL] %s: encryption failed\n", tv->name);
        return 1;
    }

    /* Compare ciphertext if expected value is provided */
    if (tv->ct_hex) {
        if (ct_len != out_len || memcmp(out_buf, ct, ct_len) != 0) {
            fprintf(stderr, "  [FAIL] %s: ciphertext mismatch\n", tv->name);
            return 1;
        }
    }

    /* Compare tag if given */
    if (tv->tag_hex) {
        if (memcmp(tag, exp_tag, tag_len) != 0) {
            fprintf(stderr, "  [FAIL] %s: tag mismatch\n", tv->name);
            return 1;
        }
    }

    /* ---- Decryption ---- */
    size_t dec_len = 0;
    uint8_t dec[256];

    switch (tv->mode) {
    case CIPHER_MODE_CBC:
        ret = aes_cbc_decrypt(out_buf, out_len, key, key_len, iv,
                              tv->padding, dec, &dec_len);
        break;
    case CIPHER_MODE_CTR:
        ret = aes_ctr_decrypt(out_buf, out_len, key, key_len, iv,
                              dec, &dec_len);
        break;
    case CIPHER_MODE_GCM:
        ret = aes_gcm_decrypt(out_buf, out_len,
                              (aad_len ? aad : NULL), aad_len,
                              exp_tag, tag_len,
                              key, key_len, iv, tv->iv_len,
                              dec, &dec_len);
        break;
    case CIPHER_MODE_XTS:
        ret = aes_xts_decrypt(out_buf, out_len, key, key_len, iv,
                              dec, &dec_len);
        break;
    default:
        fprintf(stderr, "  [FAIL] %s: unknown mode\n", tv->name);
        return 1;
    }

    if (ret) {
        fprintf(stderr, "  [FAIL] %s: decryption failed\n", tv->name);
        return 1;
    }

    if (dec_len != pt_len || memcmp(dec, pt, pt_len) != 0) {
        fprintf(stderr, "  [FAIL] %s: plaintext mismatch\n", tv->name);
        return 1;
    }

    printf("  [PASS] %s\n", tv->name);
    return 0;
}

static void run_all_tests(void) {
    int failed = 0, total = 0;
    for (const test_vector_t *tv = tests; tv->name != NULL; tv++) {
        total++;
        if (run_single_test(tv))
            failed++;
    }
    printf("\n%d test(s) failed out of %d.\n", failed, total);
}

int main(void) {
    run_all_tests();
    return 0;
}