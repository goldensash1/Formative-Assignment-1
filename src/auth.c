#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>

#include "auth.h"
#include "input.h"

#define SALT_LEN 16
#define PW_HASH_LEN 32
#define PBKDF2_ITERATIONS 100000
#define MIN_PASSWORD_LEN 8
#define MAX_PASSWORD_LEN 64
#define MAX_LOGIN_ATTEMPTS 3

/* One line of users.dat:  username:ROLE:salt_hex:pbkdf2_hash_hex */
typedef struct {
    char username[USERNAME_MAX];
    Role role;
    unsigned char salt[SALT_LEN];
    unsigned char hash[PW_HASH_LEN];
} UserRecord;

const char *auth_role_name(Role role) {
    return role == ROLE_ADMIN ? "ADMIN" : "LIBRARIAN";
}

static void hex_encode(const unsigned char *in, size_t len, char *out) {
    for (size_t i = 0; i < len; i++) sprintf(out + i * 2, "%02x", in[i]);
    out[len * 2] = '\0';
}

static int hex_decode(const char *in, unsigned char *out, size_t len) {
    if (strlen(in) != len * 2) return 0;
    for (size_t i = 0; i < len; i++) {
        unsigned int byte;
        if (sscanf(in + i * 2, "%2x", &byte) != 1) return 0;
        out[i] = (unsigned char)byte;
    }
    return 1;
}

/* PBKDF2-HMAC-SHA256: a deliberately slow, salted password hash, so a stolen
 * users.dat cannot be brute-forced cheaply and equal passwords never produce
 * equal hashes. */
static int derive_password_hash(const char *password, const unsigned char salt[SALT_LEN],
                                unsigned char out[PW_HASH_LEN]) {
    return PKCS5_PBKDF2_HMAC(password, (int)strlen(password), salt, SALT_LEN,
                             PBKDF2_ITERATIONS, EVP_sha256(), PW_HASH_LEN, out) == 1;
}

static int parse_user_line(char *line, UserRecord *rec) {
    char *fields[4];
    int n = 0;
    for (char *tok = strtok(line, ":\r\n"); tok && n < 4; tok = strtok(NULL, ":\r\n")) {
        fields[n++] = tok;
    }
    if (n != 4 || !crypto_valid_key_name(fields[0])) return 0;

    strncpy(rec->username, fields[0], sizeof(rec->username) - 1);
    rec->username[sizeof(rec->username) - 1] = '\0';

    if (strcmp(fields[1], "ADMIN") == 0) rec->role = ROLE_ADMIN;
    else if (strcmp(fields[1], "LIBRARIAN") == 0) rec->role = ROLE_LIBRARIAN;
    else return 0;

    return hex_decode(fields[2], rec->salt, SALT_LEN) &&
           hex_decode(fields[3], rec->hash, PW_HASH_LEN);
}

/* Returns 1 and fills `out` if `username` exists in the users file. */
static int find_user(const char *users_path, const char *username, UserRecord *out) {
    FILE *fp = fopen(users_path, "r");
    if (!fp) return 0;

    char line[256];
    int found = 0;
    while (!found && fgets(line, sizeof(line), fp)) {
        UserRecord rec;
        if (parse_user_line(line, &rec) && strcmp(rec.username, username) == 0) {
            *out = rec;
            found = 1;
        }
    }
    fclose(fp);
    return found;
}

int auth_has_users(const char *users_path) {
    FILE *fp = fopen(users_path, "r");
    if (!fp) return 0;

    char line[256];
    int any = 0;
    while (!any && fgets(line, sizeof(line), fp)) {
        UserRecord rec;
        any = parse_user_line(line, &rec);
    }
    fclose(fp);
    return any;
}

static int append_user(const char *users_path, const UserRecord *rec) {
    /* 0600: password hashes are readable only by the owner. */
    int fd = open(users_path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd < 0) return 0;
    FILE *fp = fdopen(fd, "a");
    if (!fp) {
        close(fd);
        return 0;
    }

    char salt_hex[SALT_LEN * 2 + 1], hash_hex[PW_HASH_LEN * 2 + 1];
    hex_encode(rec->salt, SALT_LEN, salt_hex);
    hex_encode(rec->hash, PW_HASH_LEN, hash_hex);

    int ok = fprintf(fp, "%s:%s:%s:%s\n", rec->username, auth_role_name(rec->role),
                     salt_hex, hash_hex) > 0;
    if (fclose(fp) != 0) ok = 0;
    return ok;
}

/* Asks for a new password twice. Returns 1 when a valid, confirmed password
 * is in `password`. */
static int prompt_new_password(char *password, size_t size) {
    char confirm[MAX_PASSWORD_LEN + 2];

    for (;;) {
        int rc = read_password("New password (min 8 characters): ", password, size);
        if (rc == INPUT_EOF) return 0;
        if (rc == INPUT_TOO_LONG) continue;
        if (strlen(password) < MIN_PASSWORD_LEN) {
            printf("ERROR: Password must be at least %d characters\n", MIN_PASSWORD_LEN);
            continue;
        }

        rc = read_password("Confirm password: ", confirm, sizeof(confirm));
        if (rc == INPUT_EOF) return 0;
        int match = (rc == INPUT_OK && strcmp(password, confirm) == 0);
        OPENSSL_cleanse(confirm, sizeof(confirm));
        if (match) return 1;
        printf("ERROR: Passwords do not match, try again\n");
    }
}

static int create_user(const char *users_path, const char *username,
                       const char *password, Role role) {
    UserRecord rec;
    memset(&rec, 0, sizeof(rec));
    strncpy(rec.username, username, sizeof(rec.username) - 1);
    rec.role = role;

    if (RAND_bytes(rec.salt, SALT_LEN) != 1 ||
        !derive_password_hash(password, rec.salt, rec.hash)) {
        printf("ERROR: Failed to hash password\n");
        return 0;
    }

    /* Generate the signing key first so we never store an account that has
     * no key to sign with. */
    if (!crypto_generate_user_keys(username, password)) return 0;

    if (!append_user(users_path, &rec)) {
        printf("ERROR: Could not write user file '%s'\n", users_path);
        return 0;
    }
    return 1;
}

int auth_create_user_interactive(const char *users_path, Role role) {
    char username[USERNAME_MAX + 2];
    char password[MAX_PASSWORD_LEN + 2];

    for (;;) {
        int rc = read_line("New username (3-31 letters, digits or _): ", username, sizeof(username));
        if (rc == INPUT_EOF) return 0;
        if (rc == INPUT_TOO_LONG) continue;
        if (!crypto_valid_key_name(username)) {
            printf("ERROR: Invalid username\n");
            continue;
        }
        UserRecord existing;
        if (find_user(users_path, username, &existing)) {
            printf("ERROR: User '%s' already exists\n", username);
            continue;
        }
        break;
    }

    if (!prompt_new_password(password, sizeof(password))) return 0;

    printf("Generating ECDSA key pair for '%s'...\n", username);
    int ok = create_user(users_path, username, password, role);
    OPENSSL_cleanse(password, sizeof(password));

    if (ok) {
        printf("SUCCESS: %s account '%s' created (keys: %s/%s_private.pem [encrypted], "
               "%s/%s_public.pem)\n", auth_role_name(role), username,
               KEYS_DIR, username, KEYS_DIR, username);
    }
    return ok;
}

int auth_first_run_setup(const char *users_path) {
    printf("\n=== First-run setup ===\n");
    printf("No user accounts exist yet. Create the administrator account.\n");
    return auth_create_user_interactive(users_path, ROLE_ADMIN);
}

int auth_login(const char *users_path, Session *session) {
    memset(session, 0, sizeof(*session));
    printf("\n=== Login ===\n");

    for (int attempt = 1; attempt <= MAX_LOGIN_ATTEMPTS; attempt++) {
        char username[USERNAME_MAX + 2];
        char password[MAX_PASSWORD_LEN + 2];

        int rc = read_line("Username: ", username, sizeof(username));
        if (rc == INPUT_EOF) return 0;
        rc = read_password("Password: ", password, sizeof(password));
        if (rc == INPUT_EOF) return 0;

        UserRecord rec;
        int known = find_user(users_path, username, &rec);

        /* Hash even for unknown users so response time doesn't reveal which
         * usernames exist. */
        unsigned char candidate[PW_HASH_LEN];
        unsigned char dummy_salt[SALT_LEN] = {0};
        int hashed = derive_password_hash(password, known ? rec.salt : dummy_salt, candidate);
        int ok = known && hashed && CRYPTO_memcmp(candidate, rec.hash, PW_HASH_LEN) == 0;

        EVP_PKEY *key = NULL;
        if (ok) {
            key = crypto_load_signing_key(rec.username, password);
            if (!key) {
                printf("ERROR: Could not unlock signing key for '%s' (key file missing or corrupted)\n",
                       rec.username);
                ok = 0;
            }
        }
        OPENSSL_cleanse(password, sizeof(password));

        if (ok) {
            strncpy(session->username, rec.username, sizeof(session->username) - 1);
            session->role = rec.role;
            session->signing_key = key;
            printf("Welcome, %s (%s).\n", session->username, auth_role_name(session->role));
            return 1;
        }

        printf("ERROR: Invalid username or password (%d attempt(s) left)\n",
               MAX_LOGIN_ATTEMPTS - attempt);
    }
    return 0;
}

void auth_logout(Session *session) {
    if (session->signing_key) {
        EVP_PKEY_free(session->signing_key);
        session->signing_key = NULL;
    }
}
