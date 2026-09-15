#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <time.h>
#include "registry.h"
#include "crypto.h"

#define GENESIS_HASH "0000000000000000000000000000000000000000000000000000000000000"

typedef struct {
    int index;
    time_t timestamp;
    char book_id[20];
    char book_title[80];
    char member_id[20];
    char member_name[50];
    char action[10];              /* BORROWED, RETURNED, OVERDUE */
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

/* Initializes an empty chain in memory. */
void chain_init(Blockchain *chain);

/* Frees the dynamic block array. */
void chain_free(Blockchain *chain);

/* Creates the genesis block (index 0, previous_hash = 64 zeros) and appends
 * it to the chain. Must be called once, right after chain_init, when the
 * chain is not loaded from an existing file. */
void chain_create_genesis(Blockchain *chain);

/* Appends a new lending block (BORROWED / RETURNED / OVERDUE), signs it and
 * computes its hash. Returns 1 on success, 0 on failure (signing error). */
int chain_add_block(Blockchain *chain, const char *book_id, const char *book_title,
                     const char *member_id, const char *member_name, const char *action);

/* Scans the chain (from the end) for the most recent BORROWED block for
 * `book_id` that has not since been RETURNED. Returns the block pointer, or
 * NULL if the book is not currently on loan. */
Block *chain_find_active_loan(Blockchain *chain, const char *book_id);

/* Validates hash correctness and chain linkage. Returns CHAIN_VALID if the
 * whole chain is intact; otherwise returns the failure reason and sets
 * *bad_index to the first offending block's index. */
ChainValidationResult chain_validate(Blockchain *chain, int *bad_index);

/* Prints every block's lending record to stdout, including signature
 * validity. */
void chain_print_records(Blockchain *chain);

/* Persists the chain to a binary file. Returns 1 on success. */
int chain_save(Blockchain *chain, const char *path);

/* Loads the chain from a binary file. Returns 1 on success (including the
 * case where the file does not exist yet -- the chain is simply left
 * empty so the caller can create a genesis block). */
int chain_load(Blockchain *chain, const char *path);

/* Deliberately corrupts a block's action field WITHOUT recomputing its hash,
 * for the tamper-detection demo. Returns 1 if the index was valid. */
int chain_tamper(Blockchain *chain, int index, const char *new_action);

#endif
