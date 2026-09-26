#ifndef AUTH_H
#define AUTH_H

#include <openssl/evp.h>
#include "crypto.h"

#define USERNAME_MAX KEY_NAME_MAX

typedef enum {
    ROLE_LIBRARIAN = 0,   /* borrow / return / overdue / view / validate */
    ROLE_ADMIN            /* everything above + tamper demo + add users */
} Role;

/* The authenticated user for this run of the program. */
typedef struct {
    char username[USERNAME_MAX];
    Role role;
    EVP_PKEY *signing_key;   /* decrypted with the user's password at login */
} Session;

/* Returns "ADMIN" or "LIBRARIAN". */
const char *auth_role_name(Role role);

/* Returns 1 if the users file holds at least one account. */
int auth_has_users(const char *users_path);

/* First-run setup: interactively creates the initial ADMIN account (and its
 * key pair). Returns 1 on success. */
int auth_first_run_setup(const char *users_path);

/* Interactively creates a new account with the given role. Only callable by
 * an ADMIN session (checked by the caller). Returns 1 on success. */
int auth_create_user_interactive(const char *users_path, Role role);

/* Prompts for username/password (max 3 attempts). On success fills `session`
 * (including the unlocked signing key) and returns 1. */
int auth_login(const char *users_path, Session *session);

/* Frees the signing key held by the session. */
void auth_logout(Session *session);

#endif
