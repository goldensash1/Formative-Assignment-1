#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blockchain.h"

#define INITIAL_CAPACITY 16

void chain_init(Blockchain *chain) {
    chain->blocks = malloc(sizeof(Block) * INITIAL_CAPACITY);
    chain->count = 0;
    chain->capacity = INITIAL_CAPACITY;
}

void chain_free(Blockchain *chain) {
    free(chain->blocks);
    chain->blocks = NULL;
    chain->count = 0;
    chain->capacity = 0;
}

static void chain_grow(Blockchain *chain) {
    if (chain->count >= chain->capacity) {
        chain->capacity *= 2;
        chain->blocks = realloc(chain->blocks, sizeof(Block) * chain->capacity);
    }
}

/* Builds the canonical byte buffer used both for the "data hash" (the part
 * that gets signed) and, together with the signature, for the block's final
 * hash. When include_signature is 0 the signature bytes are left out, which
 * is exactly the pre-signature "data to sign" view of the block. */
static int build_hash_buffer(const Block *b, int include_signature,
                              unsigned char *buf, size_t buf_size) {
    int n = snprintf((char *)buf, buf_size,
                      "%d|%ld|%s|%s|%s|%s|%s|%s",
                      b->index, (long)b->timestamp,
                      b->book_id, b->book_title,
                      b->member_id, b->member_name,
                      b->action, b->previous_hash);
    if (n < 0 || (size_t)n >= buf_size) return -1;

    if (include_signature && b->sig_len > 0) {
        size_t remaining = buf_size - (size_t)n;
        if (b->sig_len > remaining) return -1;
        memcpy(buf + n, b->signature, b->sig_len);
        n += (int)b->sig_len;
    }
    return n;
}

static void compute_data_hash(const Block *b, char out_hex[65]) {
    unsigned char buf[512];
    int len = build_hash_buffer(b, 0, buf, sizeof(buf));
    sha256_hex(buf, (size_t)len, out_hex);
}

static void compute_final_hash(const Block *b, char out_hex[65]) {
    unsigned char buf[512];
    int len = build_hash_buffer(b, 1, buf, sizeof(buf));
    sha256_hex(buf, (size_t)len, out_hex);
}

void chain_create_genesis(Blockchain *chain) {
    chain_grow(chain);
    Block *b = &chain->blocks[chain->count];
    memset(b, 0, sizeof(Block));

    b->index = 0;
    b->timestamp = time(NULL);
    strcpy(b->book_id, "N/A");
    strcpy(b->book_title, "GENESIS BLOCK");
    strcpy(b->member_id, "N/A");
    strcpy(b->member_name, "SYSTEM");
    strcpy(b->action, "GENESIS");
    strcpy(b->previous_hash, GENESIS_HASH);

    char data_hash[65];
    compute_data_hash(b, data_hash);
    crypto_sign_hash(data_hash, b->signature, &b->sig_len);

    char final_hash[65];
    compute_final_hash(b, final_hash);
    strcpy(b->hash, final_hash);

    chain->count++;
}

int chain_add_block(Blockchain *chain, const char *book_id, const char *book_title,
                     const char *member_id, const char *member_name, const char *action) {
    if (chain->count == 0) return 0; /* genesis must exist first */

    chain_grow(chain);
    Block *b = &chain->blocks[chain->count];
    memset(b, 0, sizeof(Block));

    Block *prev = &chain->blocks[chain->count - 1];

    b->index = chain->count;
    b->timestamp = time(NULL);
    strncpy(b->book_id, book_id, sizeof(b->book_id) - 1);
    strncpy(b->book_title, book_title, sizeof(b->book_title) - 1);
    strncpy(b->member_id, member_id, sizeof(b->member_id) - 1);
    strncpy(b->member_name, member_name, sizeof(b->member_name) - 1);
    strncpy(b->action, action, sizeof(b->action) - 1);
    strcpy(b->previous_hash, prev->hash);

    char data_hash[65];
    compute_data_hash(b, data_hash);
    if (!crypto_sign_hash(data_hash, b->signature, &b->sig_len)) {
        return 0;
    }

    char final_hash[65];
    compute_final_hash(b, final_hash);
    strcpy(b->hash, final_hash);

    chain->count++;
    return 1;
}

Block *chain_find_active_loan(Blockchain *chain, const char *book_id) {
    for (int i = chain->count - 1; i >= 0; i--) {
        Block *b = &chain->blocks[i];
        if (strcmp(b->book_id, book_id) != 0) continue;
        if (strcmp(b->action, "BORROWED") == 0) return b;
        if (strcmp(b->action, "RETURNED") == 0) return NULL;
    }
    return NULL;
}

ChainValidationResult chain_validate(Blockchain *chain, int *bad_index) {
    for (int i = 0; i < chain->count; i++) {
        Block *b = &chain->blocks[i];

        char recomputed[65];
        compute_final_hash(b, recomputed);
        if (strcmp(recomputed, b->hash) != 0) {
            if (bad_index) *bad_index = i;
            return CHAIN_BAD_HASH;
        }

        if (i > 0) {
            Block *prev = &chain->blocks[i - 1];
            if (strcmp(b->previous_hash, prev->hash) != 0) {
                if (bad_index) *bad_index = i;
                return CHAIN_BROKEN_LINK;
            }
        } else {
            if (strcmp(b->previous_hash, GENESIS_HASH) != 0) {
                if (bad_index) *bad_index = i;
                return CHAIN_BROKEN_LINK;
            }
        }

        char data_hash[65];
        compute_data_hash(b, data_hash);
        if (!crypto_verify_hash(data_hash, b->signature, b->sig_len)) {
            if (bad_index) *bad_index = i;
            return CHAIN_BAD_SIGNATURE;
        }
    }
    return CHAIN_VALID;
}

static void print_timestamp(time_t t) {
    char buf[32];
    struct tm *tm_info = localtime(&t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
    printf("%s", buf);
}

void chain_print_records(Blockchain *chain) {
    if (chain->count == 0) {
        printf("(no blocks in chain)\n");
        return;
    }

    printf("\n%-5s %-20s %-25s %-20s %-12s %-19s %-10s\n",
           "Index", "Book Title", "Member", "Action", "SigValid", "Timestamp", "Hash(8)");
    printf("--------------------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < chain->count; i++) {
        Block *b = &chain->blocks[i];

        char data_hash[65];
        compute_data_hash(b, data_hash);
        int sig_ok = crypto_verify_hash(data_hash, b->signature, b->sig_len);

        printf("%-5d %-20.20s %-25.25s %-20.20s %-12s ", b->index, b->book_title,
               b->member_name, b->action, sig_ok ? "VALID" : "INVALID");
        print_timestamp(b->timestamp);
        printf(" %.8s\n", b->hash);
    }
    printf("\n");
}

int chain_save(Blockchain *chain, const char *path) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return 0;

    fwrite(&chain->count, sizeof(int), 1, fp);
    fwrite(chain->blocks, sizeof(Block), chain->count, fp);
    fclose(fp);
    return 1;
}

int chain_load(Blockchain *chain, const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return 1; /* no file yet: caller creates genesis */

    int count = 0;
    if (fread(&count, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return 1;
    }

    while (count > chain->capacity) {
        chain->capacity *= 2;
        chain->blocks = realloc(chain->blocks, sizeof(Block) * chain->capacity);
    }

    size_t read = fread(chain->blocks, sizeof(Block), (size_t)count, fp);
    fclose(fp);

    if ((int)read != count) {
        printf("ERROR: Chain file '%s' appears truncated or corrupted\n", path);
        chain->count = (int)read;
        return 0;
    }

    chain->count = count;
    return 1;
}

int chain_tamper(Blockchain *chain, int index, const char *new_action) {
    if (index < 0 || index >= chain->count) return 0;
    strncpy(chain->blocks[index].action, new_action, sizeof(chain->blocks[index].action) - 1);
    chain->blocks[index].action[sizeof(chain->blocks[index].action) - 1] = '\0';
    /* Deliberately NOT recomputing the hash -- this is the tamper demo. */
    return 1;
}
