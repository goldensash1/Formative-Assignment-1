#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <time.h>
#include "registry.h"
#include "crypto.h"

/* previous_hash of the genesis block: exactly 64 zeros. */
#define GENESIS_HASH "0000000000000000000000000000000000000000000000000000000000000000"
_Static_assert(sizeof(GENESIS_HASH) == SHA256_HEX_LEN + 1, "GENESIS_HASH must be 64 zeros");

typedef struct {
    int index;
    time_t timestamp;
    char book_id[20];
    char book_title[80];
    char member_id[20];
    char member_name[50];
    char action[10];              /* BORROWED, RETURNED, OVERDUE (GENESIS for block 0) */
    char recorded_by[KEY_NAME_MAX]; /* authenticated user whose key signed this block */
    char previous_hash[65];
    unsigned char signature[MAX_SIGNATURE_LEN];
    unsigned int sig_len;
    char hash[65];
} Block;

typedef struct {
    Block *blocks;
    int count;
    int capacity;
} Blockchain;

typedef enum {
    CHAIN_VALID = 0,
    CHAIN_BAD_HASH,
    CHAIN_BROKEN_LINK,
    CHAIN_BAD_SIGNATURE
} ChainValidationResult;

/* Initializes an empty chain in memory. Returns 0 if allocation fails. */
int chain_init(Blockchain *chain);

/* Frees the dynamic block array. */
void chain_free(Blockchain *chain);

/* Creates the genesis block (index 0, previous_hash = 64 zeros), signed by
 * `signer`, and appends it. Returns 1 on success. */
int chain_create_genesis(Blockchain *chain, const char *signer, EVP_PKEY *signing_key);

/* Appends a new lending block (BORROWED / RETURNED / OVERDUE), signs it with
 * `signing_key` on behalf of `signer`, and computes its hash.
 * Returns 1 on success, 0 on failure. */
int chain_add_block(Blockchain *chain, const char *book_id, const char *book_title,
                    const char *member_id, const char *member_name, const char *action,
                    const char *signer, EVP_PKEY *signing_key);

/* Returns the BORROWED block of `book_id`'s current loan (a later OVERDUE
 * does not end the loan, a RETURNED does), or NULL if it is not on loan. */
Block *chain_find_active_loan(Blockchain *chain, const char *book_id);

/* Returns the most recent block about `book_id`, or NULL if there is none. */
Block *chain_latest_for_book(Blockchain *chain, const char *book_id);

/* Validates every block: (a) stored hash == recomputed hash, (b) previous_hash
 * == preceding block's hash (64 zeros for genesis), (c) signature verifies
 * with the signer's public key. Returns CHAIN_VALID or the first failure,
 * with *bad_index set to the offending block. */
ChainValidationResult chain_validate(Blockchain *chain, int *bad_index);

/* Prints every block's lending record to stdout, including signature
 * validity. */
void chain_print_records(Blockchain *chain);

/* Persists the chain to a binary file (written to a temp file, then renamed,
 * so a crash can never leave a half-written chain). Returns 1 on success. */
int chain_save(Blockchain *chain, const char *path);

/* Loads the chain from a binary file. Returns 1 on success (including the
 * case where the file does not exist yet -- the chain is simply left
 * empty so the caller can create a genesis block). */
int chain_load(Blockchain *chain, const char *path);

/* Deliberately corrupts a block's action field WITHOUT recomputing its hash,
 * for the tamper-detection demo. Returns 1 if the index was valid. */
int chain_tamper(Blockchain *chain, int index, const char *new_action);

#endif
