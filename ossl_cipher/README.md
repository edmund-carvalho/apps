# CipherGen - Simple OpenSSL AES Wrapper

A lightweight, user-friendly demo that wraps OpenSSL EVP to provide **high-level one-shot AES encryption/decryption** functions for CBC, CTR, GCM, and XTS modes.  

> [!NOTE]
> You can find the official test applications in the wiki page https://wiki.openssl.org
> I am merely trying to create wrappers to demonstrating how to use in test envoronment

## Features

- **Easy to use** - just one function call to encrypt or decrypt.
- **Multiple AES modes** - CBC, CTR, GCM, XTS.
- **All AES key sizes** - 128, 192, 256 bits (XTS uses double-length keys).
- **GCM support** - authenticated encryption with Additional Authenticated Data (AAD) and variable tag length.
- **CBC padding control** - enable/disable PKCS#7 padding.
- **Robust error handling** - OpenSSL errors are printed automatically, functions return clear success/failure.
- **Minimal dependencies** - only OpenSSL and the C standard library.
- **Built-in tests** - hard-coded NIST test vectors, run with a single command.

## Requirements

- **OpenSSL 1.1.1+** (or 3.x) development libraries and headers.  
  On Debian/Ubuntu: `sudo apt install libssl-dev`
- A C99 compiler (GCC, Clang, etc.)

## Build

Compile the library and the test application together:

```bash
gcc -std=c99 -o testapp cryptogen.c testapp.c -lssl -lcrypto
```

This creates an executable named `testapp`.

## API Overview

All functions return `0` on success, `-1` on failure. Errors are printed to `stderr` automatically (from the OpenSSL error queue).

| Function | Description |
|----------|-------------|
| `aes_cbc_encrypt` / `aes_cbc_decrypt` | AES-CBC with configurable padding |
| `aes_ctr_encrypt` / `aes_ctr_decrypt` | AES-CTR (stream mode) |
| `aes_gcm_encrypt` / `aes_gcm_decrypt` | AES-GCM with AAD and authentication tag |
| `aes_xts_encrypt` / `aes_xts_decrypt` | AES-XTS (disk encryption mode) |

### Typical usage

```c
#include "cryptogen.h"

uint8_t key[16] = { ... };
uint8_t iv[16]  = { ... };
uint8_t plaintext[] = "Hello, World!";
uint8_t ciphertext[256];
size_t ct_len;

// Encrypt
if (aes_cbc_encrypt(plaintext, strlen((char*)plaintext),
                    key, sizeof(key), iv, 1,          // enable padding
                    ciphertext, &ct_len) != 0) {
    // handle error
}

// Decrypt
uint8_t decrypted[256];
size_t pt_len;
if (aes_cbc_decrypt(ciphertext, ct_len,
                    key, sizeof(key), iv, 1,
                    decrypted, &pt_len) != 0) {
    // handle error
}
```

For **GCM**, you also provide AAD and a tag buffer:

```c
uint8_t tag[16];
aes_gcm_encrypt(plaintext, pt_len, aad, aad_len,
                key, key_len, iv, iv_len,
                ciphertext, &ct_len, tag, sizeof(tag));
```

All functions are documented in `cryptogen.h` with Doxygen-style comments.

## Running Tests

Just execute the built binary:

```bash
./testapp
```

It will run all four test vectors and print PASS/FAIL for each:

```
  [PASS] CBC-128 (no pad)
  [PASS] CTR-256
  [PASS] GCM-128
  [PASS] XTS-128

0 test(s) failed out of 4.
```

The tests verify:
- Known ciphertext (for CBC and CTR)
- Successful round-trip encryption/decryption (all modes)
- GCM authentication tag (both generation and verification)

## Adding New Test Vectors

Edit `testapp.c` and add a new entry to the `tests[]` array.  
Example:

```c
{ "My-CBC-256", CIPHER_MODE_CBC, 256,
  "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
  "000102030405060708090a0b0c0d0e0f", 0,
  "6bc1bee22e409f96e93d7e117393172a",
  NULL,               // set to NULL if no expected ciphertext
  NULL, NULL, -1 }
```

- Set `ct_hex` to `NULL` to perform only a round-trip check.
- For GCM, provide `aad_hex` and `tag_hex`.

## Advanced (Low-Level API)

The header also exposes a generic streaming `CipherCtx` API for more control (multiple updates, incremental processing).  
See the `cipher_ctx_*` functions in `cryptogen.h`. The convenience functions are built on top of it.


## Quick Reference Table
| Mode | Key | IV  | AAD | Tag | Padding |
|------|-----|-----|-----|-----|---------|
| CBC | ✅ (16/24/32 bytes) | ✅ (16 bytes) | ❌ | ❌ | ✅ (0 = off, 1 = PKCS#7) |
| CTR | ✅ (16/24/32 bytes) | ✅ (16 bytes) | ❌ | ❌ | N/A |
| GCM | ✅ (16/24/32 bytes) | ✅ (typical 12 bytes, variable)  | ✅ (can be empty) | ✅ (encrypt returns, decrypt verifies) | N/A            |
| XTS | ✅ (double‑length: 32 or 64 bytes) | ✅ (16 bytes) | ❌ | ❌ | N/A            |


## License

This code is provided as-is, free for any use. No warranty.