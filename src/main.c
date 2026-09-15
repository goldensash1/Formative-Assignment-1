#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "registry.h"
#include "crypto.h"
#include "blockchain.h"

#define BOOKS_FILE "books.txt"
#define MEMBERS_FILE "members.txt"
#define CHAIN_FILE "chain.dat"
#define PRIV_KEY_FILE "ec_private.pem"
#define PUB_KEY_FILE "ec_public.pem"

static void read_line(char *buf, int size) {
    if (fgets(buf, size, stdin) == NULL) {
        buf[0] = '\0';
        return;
    }
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
}

static void to_upper_trim(char *s) {
    int j = 0;
    for (int i = 0; s[i]; i++) {
        if (!isspace((unsigned char)s[i])) s[j++] = (char)toupper((unsigned char)s[i]);
    }
    s[j] = '\0';
}

static void print_menu(void) {
    printf("\n===== Blockchain Library Lending Tracker =====\n");
    printf("1. Borrow Book\n");
    printf("2. Return Book\n");
    printf("3. View Lending Records\n");
    printf("4. Validate Chain\n");
    printf("5. Tamper With a Block (demo)\n");
    printf("6. Exit\n");
    printf("Choose an option: ");
}

static void do_borrow(Blockchain *chain, Registry *reg) {
    char book_id[20], member_id[20];

    printf("Enter Book ID: ");
    read_line(book_id, sizeof(book_id));
    to_upper_trim(book_id);

    printf("Enter Member ID: ");
    read_line(member_id, sizeof(member_id));
    to_upper_trim(member_id);

    Book *book = registry_find_book(reg, book_id);
    Member *member = registry_find_member(reg, member_id);

    if (!book || !member) {
        printf("ERROR: Book or Member not found\n");
        return;
    }

    if (chain_find_active_loan(chain, book_id) != NULL) {
        printf("ERROR: Book '%s' is already on loan and has not been returned\n", book->title);
        return;
    }

    if (!chain_add_block(chain, book->book_id, book->title, member->member_id,
                          member->full_name, "BORROWED")) {
        printf("ERROR: Failed to create and sign block\n");
        return;
    }

    chain_save(chain, CHAIN_FILE);
    printf("SUCCESS: '%s' borrowed by %s. Block #%d added to the chain.\n",
           book->title, member->full_name, chain->count - 1);
}

static void do_return(Blockchain *chain, Registry *reg) {
    char book_id[20];
    printf("Enter Book ID: ");
    read_line(book_id, sizeof(book_id));
    to_upper_trim(book_id);

    Book *book = registry_find_book(reg, book_id);
    if (!book) {
        printf("ERROR: Book or Member not found\n");
        return;
    }

    Block *loan = chain_find_active_loan(chain, book_id);
    if (!loan) {
        printf("ERROR: No active loan found for book '%s' (never borrowed, or already returned)\n",
               book->title);
        return;
    }

    Member *member = registry_find_member(reg, loan->member_id);
    const char *member_name = member ? member->full_name : loan->member_name;
    const char *member_id = member ? member->member_id : loan->member_id;

    if (!chain_add_block(chain, book->book_id, book->title, member_id,
                          member_name, "RETURNED")) {
        printf("ERROR: Failed to create and sign block\n");
        return;
    }

    chain_save(chain, CHAIN_FILE);
    printf("SUCCESS: '%s' returned by %s. Block #%d added to the chain.\n",
           book->title, member_name, chain->count - 1);
}

static void do_validate(Blockchain *chain) {
    int bad_index = -1;
    ChainValidationResult result = chain_validate(chain, &bad_index);

    switch (result) {
        case CHAIN_VALID:
            printf("Chain is VALID. All %d blocks verified: hashes correct, links intact, "
                   "signatures valid.\n", chain->count);
            break;
        case CHAIN_BAD_HASH:
            printf("Chain is INVALID: block #%d's stored hash does not match its recomputed "
                   "hash (data was tampered with).\n", bad_index);
            break;
        case CHAIN_BROKEN_LINK:
            printf("Chain is INVALID: block #%d's previous_hash does not match the preceding "
                   "block's hash (chain link broken).\n", bad_index);
            break;
        case CHAIN_BAD_SIGNATURE:
            printf("Chain is INVALID: block #%d's digital signature does not verify.\n", bad_index);
            break;
    }
}

static void do_tamper(Blockchain *chain) {
    if (chain->count == 0) {
        printf("Chain is empty, nothing to tamper with.\n");
        return;
    }

    char idx_str[16];
    printf("Enter block index to tamper with (0-%d): ", chain->count - 1);
    read_line(idx_str, sizeof(idx_str));
    int index = atoi(idx_str);

    char new_action[10];
    printf("Enter new action value to inject (e.g. RETURNED): ");
    read_line(new_action, sizeof(new_action));

    if (chain_tamper(chain, index, new_action)) {
        printf("Block #%d's action field was silently modified to '%s' (hash NOT recomputed).\n",
               index, new_action);
        printf("Run 'Validate Chain' to see tamper detection in action.\n");
        printf("(This in-memory change is not saved to disk, so restarting the program\n"
               " restores the original chain. Use 'Validate Chain' now to observe it.)\n");
    } else {
        printf("ERROR: Invalid block index\n");
    }
}

int main(void) {
    printf("Loading registries...\n");
    Registry reg;
    if (!registry_load(&reg, BOOKS_FILE, MEMBERS_FILE)) {
        printf("FATAL: Could not load book/member registries. Exiting.\n");
        return 1;
    }
    printf("Loaded %d books and %d members.\n", reg.book_count, reg.member_count);

    printf("Initializing ECDSA key pair...\n");
    if (!crypto_init_keys(PRIV_KEY_FILE, PUB_KEY_FILE)) {
        printf("FATAL: Could not initialize cryptographic keys. Exiting.\n");
        return 1;
    }

    Blockchain chain;
    chain_init(&chain);
    if (!chain_load(&chain, CHAIN_FILE)) {
        printf("FATAL: Could not load chain file. Exiting.\n");
        crypto_cleanup_keys();
        return 1;
    }

    if (chain.count == 0) {
        printf("No existing chain found. Creating genesis block...\n");
        chain_create_genesis(&chain);
        chain_save(&chain, CHAIN_FILE);
    } else {
        printf("Loaded existing chain with %d block(s).\n", chain.count);
    }

    int running = 1;
    while (running) {
        print_menu();
        char choice[16];
        read_line(choice, sizeof(choice));
        int opt = atoi(choice);

        switch (opt) {
            case 1: do_borrow(&chain, &reg); break;
            case 2: do_return(&chain, &reg); break;
            case 3: chain_print_records(&chain); break;
            case 4: do_validate(&chain); break;
            case 5: do_tamper(&chain); break;
            case 6: running = 0; break;
            default: printf("Invalid option, try again.\n"); break;
        }
    }

    printf("Goodbye.\n");
    chain_free(&chain);
    crypto_cleanup_keys();
    return 0;
}
