#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include "crypto.h"

static EVP_PKEY *g_keypair = NULL;

void sha256_hex(const unsigned char *data, size_t len, char out_hex[SHA256_HEX_LEN + 1]) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    unsigned int digest_len = 0;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, data, len);
    EVP_DigestFinal_ex(ctx, digest, &digest_len);
    EVP_MD_CTX_free(ctx);

    for (unsigned int i = 0; i < digest_len; i++) {
        sprintf(out_hex + (i * 2), "%02x", digest[i]);
    }
    out_hex[digest_len * 2] = '\0';
}

static EVP_PKEY *generate_ec_keypair(void) {
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
    if (!pctx) return NULL;

    EVP_PKEY *pkey = NULL;
    if (EVP_PKEY_keygen_init(pctx) <= 0 ||
        EVP_PKEY_CTX_set_group_name(pctx, "prime256v1") <= 0 ||
        EVP_PKEY_keygen(pctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return NULL;
    }
    EVP_PKEY_CTX_free(pctx);
    return pkey;
}

static int save_keypair(EVP_PKEY *pkey, const char *priv_path, const char *pub_path) {
    FILE *fpriv = fopen(priv_path, "w");
    if (!fpriv) return 0;
    int ok = PEM_write_PrivateKey(fpriv, pkey, NULL, NULL, 0, NULL, NULL);
    fclose(fpriv);
    if (!ok) return 0;

    FILE *fpub = fopen(pub_path, "w");
    if (!fpub) return 0;
    ok = PEM_write_PUBKEY(fpub, pkey);
    fclose(fpub);
    return ok;
}

int crypto_init_keys(const char *priv_path, const char *pub_path) {
    FILE *fpriv = fopen(priv_path, "r");
    if (fpriv) {
        g_keypair = PEM_read_PrivateKey(fpriv, NULL, NULL, NULL);
        fclose(fpriv);
        if (g_keypair) return 1;
        printf("ERROR: Failed to parse existing key file '%s'\n", priv_path);
        return 0;
    }

    /* No key on disk yet: generate a fresh ECDSA (P-256) key pair. */
    printf("No ECDSA key pair found. Generating a new one (%s, %s)...\n", priv_path, pub_path);
    g_keypair = generate_ec_keypair();
    if (!g_keypair) {
        printf("ERROR: Failed to generate ECDSA key pair\n");
        return 0;
    }
    if (!save_keypair(g_keypair, priv_path, pub_path)) {
        printf("ERROR: Failed to save ECDSA key pair to disk\n");
        return 0;
    }
    return 1;
}

void crypto_cleanup_keys(void) {
    if (g_keypair) {
        EVP_PKEY_free(g_keypair);
        g_keypair = NULL;
    }
}

int crypto_sign_hash(const char hash_hex[SHA256_HEX_LEN + 1],
                      unsigned char sig_out[MAX_SIGNATURE_LEN],
                      unsigned int *sig_len_out) {
    if (!g_keypair) return 0;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    size_t sig_len = MAX_SIGNATURE_LEN;
    int ok = 0;

    if (EVP_DigestSignInit(ctx, NULL, EVP_sha256(), NULL, g_keypair) == 1 &&
        EVP_DigestSign(ctx, sig_out, &sig_len,
                        (const unsigned char *)hash_hex, strlen(hash_hex)) == 1) {
        *sig_len_out = (unsigned int)sig_len;
        ok = 1;
    }

    EVP_MD_CTX_free(ctx);
    return ok;
}

int crypto_verify_hash(const char hash_hex[SHA256_HEX_LEN + 1],
                        const unsigned char *signature,
                        unsigned int sig_len) {
    if (!g_keypair || sig_len == 0) return 0;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    int result = 0;

    if (EVP_DigestVerifyInit(ctx, NULL, EVP_sha256(), NULL, g_keypair) == 1) {
        int rc = EVP_DigestVerify(ctx, signature, sig_len,
                                   (const unsigned char *)hash_hex, strlen(hash_hex));
        result = (rc == 1);
    }

    EVP_MD_CTX_free(ctx);
    return result;
}
