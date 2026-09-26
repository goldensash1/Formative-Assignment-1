#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blockchain.h"

#define INITIAL_CAPACITY 16
#define MAX_CHAIN_BLOCKS 1000000   /* sanity bound when loading from disk */

int chain_init(Blockchain *chain) {
    chain->blocks = malloc(sizeof(Block) * INITIAL_CAPACITY);
    chain->count = 0;
    chain->capacity = chain->blocks ? INITIAL_CAPACITY : 0;
    return chain->blocks != NULL;
}

void chain_free(Blockchain *chain) {
    free(chain->blocks);
    chain->blocks = NULL;
    chain->count = 0;
    chain->capacity = 0;
}

/* Ensures room for at least `needed` blocks. Returns 0 if out of memory
 * (the existing blocks are left untouched). */
static int chain_reserve(Blockchain *chain, int needed) {
    int new_capacity = chain->capacity > 0 ? chain->capacity : INITIAL_CAPACITY;
    while (new_capacity < needed) new_capacity *= 2;
    if (new_capacity == chain->capacity) return 1;

    Block *grown = realloc(chain->blocks, sizeof(Block) * (size_t)new_capacity);
    if (!grown) {
        printf("ERROR: Out of memory while growing the chain\n");
        return 0;
    }
    chain->blocks = grown;
    chain->capacity = new_capacity;
    return 1;
}

/* Builds the canonical byte buffer used both for the "data hash" (the part
 * that gets signed) and, together with the signature, for the block's final
 * hash. When include_signature is 0 the signature bytes are left out, which
 * is exactly the pre-signature "data to sign" view of the block. */
static int build_hash_buffer(const Block *b, int include_signature,
                             unsigned char *buf, size_t buf_size) {
    int n = snprintf((char *)buf, buf_size,
                     "%d|%ld|%s|%s|%s|%s|%s|%s|%s",
                     b->index, (long)b->timestamp,
                     b->book_id, b->book_title,
                     b->member_id, b->member_name,
                     b->action, b->recorded_by, b->previous_hash);
    if (n < 0 || (size_t)n >= buf_size) return -1;

    if (include_signature && b->sig_len > 0) {
        size_t remaining = buf_size - (size_t)n;
        if (b->sig_len > MAX_SIGNATURE_LEN || b->sig_len > remaining) return -1;
        memcpy(buf + n, b->signature, b->sig_len);
        n += (int)b->sig_len;
    }
    return n;
}

static void compute_hash(const Block *b, int include_signature, char out_hex[65]) {
    unsigned char buf[512];
    int len = build_hash_buffer(b, include_signature, buf, sizeof(buf));
    if (len < 0) {
        /* Malformed block (e.g. unterminated strings from a corrupted file):
         * produce a hash that can never match a real one. */
        out_hex[0] = '\0';
        return;
    }
    sha256_hex(buf, (size_t)len, out_hex);
}

/* Fills in previous_hash, signature and hash for a block whose data fields
 * are already set. */
static int seal_block(Block *b, const char *previous_hash, EVP_PKEY *signing_key) {
    strcpy(b->previous_hash, previous_hash);

    char data_hash[65];
    compute_hash(b, 0, data_hash);
    if (data_hash[0] == '\0' ||
        !crypto_sign_hash(signing_key, data_hash, b->signature, &b->sig_len)) {
        return 0;
    }

    compute_hash(b, 1, b->hash);
    return b->hash[0] != '\0';
}

static void copy_field(char *dst, size_t dst_size, const char *src) {
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

int chain_create_genesis(Blockchain *chain, const char *signer, EVP_PKEY *signing_key) {
    if (chain->count != 0 || !chain_reserve(chain, 1)) return 0;

    Block *b = &chain->blocks[0];
    memset(b, 0, sizeof(Block));
    b->index = 0;
    b->timestamp = time(NULL);
    copy_field(b->book_id, sizeof(b->book_id), "N/A");
    copy_field(b->book_title, sizeof(b->book_title), "GENESIS BLOCK");
    copy_field(b->member_id, sizeof(b->member_id), "N/A");
    copy_field(b->member_name, sizeof(b->member_name), "SYSTEM");
    copy_field(b->action, sizeof(b->action), "GENESIS");
    copy_field(b->recorded_by, sizeof(b->recorded_by), signer);

    if (!seal_block(b, GENESIS_HASH, signing_key)) return 0;
    chain->count = 1;
    return 1;
}

int chain_add_block(Blockchain *chain, const char *book_id, const char *book_title,
                    const char *member_id, const char *member_name, const char *action,
                    const char *signer, EVP_PKEY *signing_key) {
    if (chain->count == 0) return 0; /* genesis must exist first */
    if (!chain_reserve(chain, chain->count + 1)) return 0;

    Block *b = &chain->blocks[chain->count];
    memset(b, 0, sizeof(Block));
    b->index = chain->count;
    b->timestamp = time(NULL);
    copy_field(b->book_id, sizeof(b->book_id), book_id);
    copy_field(b->book_title, sizeof(b->book_title), book_title);
    copy_field(b->member_id, sizeof(b->member_id), member_id);
    copy_field(b->member_name, sizeof(b->member_name), member_name);
    copy_field(b->action, sizeof(b->action), action);
    copy_field(b->recorded_by, sizeof(b->recorded_by), signer);

    if (!seal_block(b, chain->blocks[chain->count - 1].hash, signing_key)) return 0;
    chain->count++;
    return 1;
}

Block *chain_find_active_loan(Blockchain *chain, const char *book_id) {
    for (int i = chain->count - 1; i >= 0; i--) {
        Block *b = &chain->blocks[i];
        if (strcmp(b->book_id, book_id) != 0) continue;
        if (strcmp(b->action, "BORROWED") == 0) return b;
        if (strcmp(b->action, "RETURNED") == 0) return NULL;
        /* OVERDUE: loan is still open, keep looking for its BORROWED block */
    }
    return NULL;
}

Block *chain_latest_for_book(Blockchain *chain, const char *book_id) {
    for (int i = chain->count - 1; i >= 0; i--) {
        if (strcmp(chain->blocks[i].book_id, book_id) == 0) return &chain->blocks[i];
    }
    return NULL;
}

static int signature_valid(const Block *b) {
    char data_hash[65];
    compute_hash(b, 0, data_hash);
    return data_hash[0] != '\0' &&
           crypto_verify_hash(b->recorded_by, data_hash, b->signature, b->sig_len);
}

ChainValidationResult chain_validate(Blockchain *chain, int *bad_index) {
    for (int i = 0; i < chain->count; i++) {
        Block *b = &chain->blocks[i];

        char recomputed[65];
        compute_hash(b, 1, recomputed);
        if (recomputed[0] == '\0' || strcmp(recomputed, b->hash) != 0) {
            if (bad_index) *bad_index = i;
            return CHAIN_BAD_HASH;
        }

        const char *expected_prev = (i == 0) ? GENESIS_HASH : chain->blocks[i - 1].hash;
        if (b->index != i || strcmp(b->previous_hash, expected_prev) != 0) {
            if (bad_index) *bad_index = i;
            return CHAIN_BROKEN_LINK;
        }

        if (!signature_valid(b)) {
            if (bad_index) *bad_index = i;
            return CHAIN_BAD_SIGNATURE;
        }
    }
    return CHAIN_VALID;
}

static void format_timestamp(time_t t, char *buf, size_t size) {
    struct tm *tm_info = localtime(&t);
    if (!tm_info || strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm_info) == 0) {
        snprintf(buf, size, "%ld", (long)t);
    }
}

void chain_print_records(Blockchain *chain) {
    if (chain->count == 0) {
        printf("(no blocks in chain)\n");
        return;
    }

    printf("\n%-5s %-6s %-20s %-7s %-16s %-9s %-12s %-8s %-19s %s\n",
           "Index", "Book", "Book Title", "Member", "Member Name", "Action",
           "Signed By", "SigValid", "Timestamp", "Hash(8)");
    for (int i = 0; i < 118; i++) putchar('-');
    putchar('\n');

    for (int i = 0; i < chain->count; i++) {
        Block *b = &chain->blocks[i];
        char ts[32];
        format_timestamp(b->timestamp, ts, sizeof(ts));

        printf("%-5d %-6.6s %-20.20s %-7.7s %-16.16s %-9.9s %-12.12s %-8s %-19s %.8s\n",
               b->index, b->book_id, b->book_title, b->member_id, b->member_name,
               b->action, b->recorded_by, signature_valid(b) ? "VALID" : "INVALID",
               ts, b->hash);
    }
    printf("\n");
}

int chain_save(Blockchain *chain, const char *path) {
    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE *fp = fopen(tmp_path, "wb");
    if (!fp) {
        printf("ERROR: Could not open '%s' for writing\n", tmp_path);
        return 0;
    }

    int ok = fwrite(&chain->count, sizeof(int), 1, fp) == 1 &&
             fwrite(chain->blocks, sizeof(Block), (size_t)chain->count, fp) == (size_t)chain->count;
    if (fclose(fp) != 0) ok = 0;

    if (!ok || rename(tmp_path, path) != 0) {
        printf("ERROR: Failed to save chain to '%s'\n", path);
        remove(tmp_path);
        return 0;
    }
    return 1;
}

int chain_load(Blockchain *chain, const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return 1; /* no file yet: caller creates genesis */

    int count = 0;
    if (fread(&count, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        printf("ERROR: Chain file '%s' is empty or unreadable\n", path);
        return 0;
    }

    if (count < 1 || count > MAX_CHAIN_BLOCKS) {
        fclose(fp);
        printf("ERROR: Chain file '%s' has an invalid block count (%d)\n", path, count);
        return 0;
    }

    if (!chain_reserve(chain, count)) {
        fclose(fp);
        return 0;
    }

    size_t read = fread(chain->blocks, sizeof(Block), (size_t)count, fp);
    fclose(fp);

    if ((int)read != count) {
        printf("ERROR: Chain file '%s' appears truncated or corrupted\n", path);
        chain->count = 0;
        return 0;
    }

    /* Force-terminate every string so a corrupted file can't cause reads
     * past the end of a field; any real change still fails validation. */
    for (int i = 0; i < count; i++) {
        Block *b = &chain->blocks[i];
        b->book_id[sizeof(b->book_id) - 1] = '\0';
        b->book_title[sizeof(b->book_title) - 1] = '\0';
        b->member_id[sizeof(b->member_id) - 1] = '\0';
        b->member_name[sizeof(b->member_name) - 1] = '\0';
        b->action[sizeof(b->action) - 1] = '\0';
        b->recorded_by[sizeof(b->recorded_by) - 1] = '\0';
        b->previous_hash[sizeof(b->previous_hash) - 1] = '\0';
        b->hash[sizeof(b->hash) - 1] = '\0';
    }

    chain->count = count;
    return 1;
}

int chain_tamper(Blockchain *chain, int index, const char *new_action) {
    if (index < 0 || index >= chain->count) return 0;
    copy_field(chain->blocks[index].action, sizeof(chain->blocks[index].action), new_action);
    /* Deliberately NOT recomputing the hash -- this is the tamper demo. */
    return 1;
}
