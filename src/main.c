#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "registry.h"
#include "crypto.h"
#include "auth.h"
#include "input.h"
#include "blockchain.h"

#define BOOKS_FILE "books.txt"
#define MEMBERS_FILE "members.txt"
#define CHAIN_FILE "chain.dat"
#define USERS_FILE "users.dat"
#define LOAN_PERIOD_DAYS 14

static void print_menu(const Session *s) {
    printf("\n===== Blockchain Library Lending Tracker =====\n");
    printf("Logged in as: %s (%s)\n", s->username, auth_role_name(s->role));
    printf("1. Borrow Book\n");
    printf("2. Return Book\n");
    printf("3. Mark Book Overdue\n");
    printf("4. View Lending Records\n");
    printf("5. Validate Chain\n");
    printf("6. Tamper With a Block (demo) [ADMIN]\n");
    printf("7. Add User Account [ADMIN]\n");
    printf("8. Exit\n");
}

static int require_admin(const Session *s) {
    if (s->role == ROLE_ADMIN) return 1;
    printf("ERROR: Access denied. This action requires the ADMIN role.\n");
    return 0;
}

static void print_validation_result(ChainValidationResult result, int bad_index, int count) {
    switch (result) {
        case CHAIN_VALID:
            printf("Chain is VALID. All %d blocks verified: hashes correct, links intact, "
                   "signatures valid.\n", count);
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
            printf("Chain is INVALID: block #%d's digital signature does not verify with "
                   "its signer's public key.\n", bad_index);
            break;
    }
}

/* Never build on top of a tampered chain: that would both hide the tampering
 * and persist it to disk on the next save. */
static int ensure_chain_intact(Blockchain *chain) {
    int bad_index = -1;
    ChainValidationResult result = chain_validate(chain, &bad_index);
    if (result == CHAIN_VALID) return 1;

    print_validation_result(result, bad_index, chain->count);
    printf("ERROR: Refusing to add blocks to an invalid chain. Restart the program to reload "
           "the saved chain, or restore '%s' from a trusted copy.\n", CHAIN_FILE);
    return 0;
}

/* Prompts for an ID and normalises it. Returns 0 if input was aborted. */
static int prompt_id(const char *prompt, char *buf, size_t size) {
    if (read_line(prompt, buf, size) != INPUT_OK) return 0;
    to_upper_trim(buf);
    return 1;
}

static int append_and_save(Blockchain *chain, const Session *s, const Book *book,
                           const char *member_id, const char *member_name, const char *action) {
    if (!chain_add_block(chain, book->book_id, book->title, member_id, member_name,
                         action, s->username, s->signing_key)) {
        printf("ERROR: Failed to create and sign block\n");
        return 0;
    }
    if (!chain_save(chain, CHAIN_FILE)) {
        printf("WARNING: Block #%d is in memory but could not be saved to disk\n",
               chain->count - 1);
    }
    return 1;
}

static void do_borrow(Blockchain *chain, Registry *reg, const Session *s) {
    char book_id[20], member_id[20];
    if (!prompt_id("Enter Book ID: ", book_id, sizeof(book_id))) return;
    if (!prompt_id("Enter Member ID: ", member_id, sizeof(member_id))) return;

    Book *book = registry_find_book(reg, book_id);
    Member *member = registry_find_member(reg, member_id);
    if (!book || !member) {
        printf("ERROR: Book or Member not found\n");
        return;
    }

    if (chain_find_active_loan(chain, book->book_id) != NULL) {
        printf("ERROR: Book '%s' is already on loan and has not been returned\n", book->title);
        return;
    }

    if (!ensure_chain_intact(chain)) return;
    if (append_and_save(chain, s, book, member->member_id, member->full_name, "BORROWED")) {
        printf("SUCCESS: '%s' borrowed by %s. Block #%d added and signed by %s.\n",
               book->title, member->full_name, chain->count - 1, s->username);
    }
}

static void do_return(Blockchain *chain, Registry *reg, const Session *s) {
    char book_id[20], member_id[20];
    if (!prompt_id("Enter Book ID: ", book_id, sizeof(book_id))) return;
    if (!prompt_id("Enter Member ID: ", member_id, sizeof(member_id))) return;

    Book *book = registry_find_book(reg, book_id);
    Member *member = registry_find_member(reg, member_id);
    if (!book || !member) {
        printf("ERROR: Book or Member not found\n");
        return;
    }

    Block *loan = chain_find_active_loan(chain, book->book_id);
    if (!loan) {
        printf("ERROR: No active loan found for book '%s' (never borrowed, or already returned)\n",
               book->title);
        return;
    }

    if (strcmp(loan->member_id, member->member_id) != 0) {
        printf("ERROR: '%s' is on loan to %s (%s), not %s (%s)\n", book->title,
               loan->member_name, loan->member_id, member->full_name, member->member_id);
        return;
    }

    if (!ensure_chain_intact(chain)) return;
    if (append_and_save(chain, s, book, member->member_id, member->full_name, "RETURNED")) {
        printf("SUCCESS: '%s' returned by %s. Block #%d added and signed by %s.\n",
               book->title, member->full_name, chain->count - 1, s->username);
    }
}

static void do_overdue(Blockchain *chain, Registry *reg, const Session *s) {
    char book_id[20];
    if (!prompt_id("Enter Book ID: ", book_id, sizeof(book_id))) return;

    Book *book = registry_find_book(reg, book_id);
    if (!book) {
        printf("ERROR: Book or Member not found\n");
        return;
    }

    Block *loan = chain_find_active_loan(chain, book->book_id);
    if (!loan) {
        printf("ERROR: Book '%s' is not currently on loan\n", book->title);
        return;
    }

    Block *latest = chain_latest_for_book(chain, book->book_id);
    if (latest && strcmp(latest->action, "OVERDUE") == 0) {
        printf("ERROR: Book '%s' is already marked OVERDUE\n", book->title);
        return;
    }

    long days = (long)((time(NULL) - loan->timestamp) / 86400);
    printf("'%s' has been on loan to %s for %ld day(s) (loan period: %d days).\n",
           book->title, loan->member_name, days, LOAN_PERIOD_DAYS);

    if (!ensure_chain_intact(chain)) return;
    if (append_and_save(chain, s, book, loan->member_id, loan->member_name, "OVERDUE")) {
        printf("SUCCESS: '%s' marked OVERDUE for %s. Block #%d added and signed by %s.\n",
               book->title, loan->member_name, chain->count - 1, s->username);
    }
}

static void do_validate(Blockchain *chain) {
    int bad_index = -1;
    ChainValidationResult result = chain_validate(chain, &bad_index);
    print_validation_result(result, bad_index, chain->count);
}

static void do_tamper(Blockchain *chain, const Session *s) {
    if (!require_admin(s)) return;

    char idx_str[16], prompt[64];
    snprintf(prompt, sizeof(prompt), "Enter block index to tamper with (0-%d): ", chain->count - 1);
    if (read_line(prompt, idx_str, sizeof(idx_str)) != INPUT_OK) return;

    int index;
    if (!parse_int(idx_str, &index) || index < 0 || index >= chain->count) {
        printf("ERROR: Invalid block index\n");
        return;
    }

    char new_action[10];
    if (read_line("Enter new action value to inject (e.g. RETURNED): ",
                  new_action, sizeof(new_action)) != INPUT_OK) return;

    chain_tamper(chain, index, new_action);
    printf("Block #%d's action field was silently modified to '%s' (hash NOT recomputed).\n",
           index, new_action);
    printf("Run 'Validate Chain' to see tamper detection in action.\n");
    printf("(The change exists only in memory. New blocks are refused while the chain is\n"
           " invalid, so it is never saved; restart the program to reload the genuine chain.)\n");
}

static void do_add_user(const Session *s) {
    if (!require_admin(s)) return;

    char choice[8];
    if (read_line("Role for new account (1 = LIBRARIAN, 2 = ADMIN): ",
                  choice, sizeof(choice)) != INPUT_OK) return;

    int role_num;
    if (!parse_int(choice, &role_num) || (role_num != 1 && role_num != 2)) {
        printf("ERROR: Invalid role\n");
        return;
    }
    auth_create_user_interactive(USERS_FILE, role_num == 2 ? ROLE_ADMIN : ROLE_LIBRARIAN);
}

int main(void) {
    printf("Loading registries...\n");
    Registry reg;
    if (!registry_load(&reg, BOOKS_FILE, MEMBERS_FILE)) {
        printf("FATAL: Could not load book/member registries. Exiting.\n");
        return 1;
    }
    printf("Loaded %d books and %d members.\n", reg.book_count, reg.member_count);

    if (!auth_has_users(USERS_FILE) && !auth_first_run_setup(USERS_FILE)) {
        printf("FATAL: Administrator account was not created. Exiting.\n");
        return 1;
    }

    Session session;
    if (!auth_login(USERS_FILE, &session)) {
        printf("FATAL: Authentication failed. Exiting.\n");
        return 1;
    }

    Blockchain chain;
    if (!chain_init(&chain) || !chain_load(&chain, CHAIN_FILE)) {
        printf("FATAL: Could not load chain file. Exiting.\n");
        chain_free(&chain);
        auth_logout(&session);
        return 1;
    }

    if (chain.count == 0) {
        printf("No existing chain found. Creating genesis block...\n");
        if (!chain_create_genesis(&chain, session.username, session.signing_key) ||
            !chain_save(&chain, CHAIN_FILE)) {
            printf("FATAL: Could not create genesis block. Exiting.\n");
            chain_free(&chain);
            auth_logout(&session);
            return 1;
        }
    } else {
        printf("Loaded existing chain with %d block(s).\n", chain.count);
    }

    printf("Verifying chain integrity on startup... ");
    do_validate(&chain);

    int running = 1;
    while (running) {
        print_menu(&session);
        char choice[16];
        int rc = read_line("Choose an option: ", choice, sizeof(choice));
        if (rc == INPUT_EOF) break;

        int opt = 0;
        if (rc == INPUT_OK && !parse_int(choice, &opt)) opt = 0;

        switch (opt) {
            case 1: do_borrow(&chain, &reg, &session); break;
            case 2: do_return(&chain, &reg, &session); break;
            case 3: do_overdue(&chain, &reg, &session); break;
            case 4: chain_print_records(&chain); break;
            case 5: do_validate(&chain); break;
            case 6: do_tamper(&chain, &session); break;
            case 7: do_add_user(&session); break;
            case 8: running = 0; break;
            default: printf("Invalid option, try again.\n"); break;
        }
        if (input_eof()) break;
    }

    printf("Goodbye.\n");
    chain_free(&chain);
    auth_logout(&session);
    crypto_cleanup();
    return 0;
}
