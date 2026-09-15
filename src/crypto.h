#ifndef CRYPTO_H
#define CRYPTO_H

#include <stddef.h>

#define SHA256_HEX_LEN 64   /* hex characters, plus '\0' = 65 */
#define MAX_SIGNATURE_LEN 72

/* Computes the SHA-256 hash of `data` (length `len`) and writes it as a
 * lowercase hex string (65 bytes incl. terminator) into `out_hex`. */
void sha256_hex(const unsigned char *data, size_t len, char out_hex[SHA256_HEX_LEN + 1]);

/* Loads the ECDSA (P-256) key pair from disk, generating and saving a new
 * pair on first run if the files do not exist yet. Returns 1 on success. */
int crypto_init_keys(const char *priv_path, const char *pub_path);

/* Releases the key material held by crypto_init_keys. */
void crypto_cleanup_keys(void);

/* Signs the SHA-256 hex digest `hash_hex` with the loaded private key.
 * Writes the DER-encoded signature into `sig_out` (capacity MAX_SIGNATURE_LEN)
 * and its length into `sig_len_out`. Returns 1 on success. */
int crypto_sign_hash(const char hash_hex[SHA256_HEX_LEN + 1],
                      unsigned char sig_out[MAX_SIGNATURE_LEN],
                      unsigned int *sig_len_out);

/* Verifies `signature` (length sig_len) against `hash_hex` using the loaded
 * public key. Returns 1 if valid, 0 otherwise. */
int crypto_verify_hash(const char hash_hex[SHA256_HEX_LEN + 1],
                        const unsigned char *signature,
                        unsigned int sig_len);

#endif
