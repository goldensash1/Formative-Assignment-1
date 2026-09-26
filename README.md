# Blockchain-Based Library Book Lending Tracker

A C implementation of an immutable, cryptographically verifiable library
lending ledger. Every borrow / return / overdue event is recorded as a block
in a hash-linked chain and digitally signed by the authenticated librarian
who recorded it, so neither a librarian nor a borrower can quietly rewrite
history.

## Features

- **Book & Member registries** loaded from `books.txt` / `members.txt` at
  startup into arrays of `Book` / `Member` structs, used to validate every
  lending action. Missing or empty files, malformed lines and duplicate IDs
  are reported.
- **Blockchain** of lending records linked by SHA-256 hashes, starting from a
  genesis block whose `previous_hash` is 64 zeros.
- **User authentication & access control**: librarians log in with a
  username and password (salted PBKDF2-HMAC-SHA256, 100,000 iterations,
  3 attempts max). Two roles: `LIBRARIAN` and `ADMIN`.
- **Per-user ECDSA (P-256) keys**: every account has its own key pair. The
  private key is stored AES-256 encrypted with the user's password (file mode
  `0600`) and is only unlocked at login. Every block is signed with the
  private key of the user who recorded it and verified with that user's
  **public** key.
- **Chain validation** detects modified data (hash mismatch), broken links and
  forged or invalid signatures. It runs automatically at startup and before
  every new block. New blocks are refused while the chain is invalid.
- **File-based persistence** (`chain.dat`, saved atomically via a temp file
  and rename) so the chain survives restarts.
- **CLI** to borrow, return, mark overdue, view records, validate the chain,
  list books (with availability) and members (with their loans), and run a
  tamper-detection demo.

## Required Libraries / Dependencies

- A C compiler (`gcc` or `clang`) and `make`
- **OpenSSL 3.x** (development headers + libcrypto/libssl), used for SHA-256,
  ECDSA signing/verification, PBKDF2, AES key encryption and secure random
  numbers.
  - macOS: `brew install openssl@3`
  - Debian/Ubuntu: `sudo apt-get install build-essential libssl-dev`

### Apple Silicon (M1/M2/M3/...) note

The `Makefile` looks for OpenSSL in `/opt/homebrew/opt/openssl@3` and
`/usr/local/opt/openssl@3` (override with
`make OPENSSL_PREFIX=/path/to/openssl`). If the only OpenSSL it finds is an
Intel (x86_64) build, it compiles an x86_64 binary, which runs under Rosetta
2. Install Rosetta once with:

```bash
softwareupdate --install-rosetta --agree-to-license
```

With a native arm64 OpenSSL (under `/opt/homebrew`), a native arm64 binary
is built automatically.

## Build & Run

```bash
make            # compiles ./library_tracker
make run        # compiles (if needed) and runs it
make clean      # removes the binary and chain.dat (keeps accounts & keys)
make distclean  # full reset: also removes users.dat and keys/
```

Or manually (macOS example):

```bash
gcc -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L \
    -I$(brew --prefix openssl@3)/include \
    -o library_tracker src/*.c \
    -L$(brew --prefix openssl@3)/lib -lssl -lcrypto
./library_tracker
```

Run the program from the project directory so it can find `books.txt` and
`members.txt`.

On first run the program will:

1. Load `books.txt` and `members.txt` into memory (it exits with an error if
   either is missing or empty).
2. Ask you to create the **administrator account** (username + password).
   This generates `keys/<username>_private.pem` (encrypted) and
   `keys/<username>_public.pem`, and stores the salted password hash in
   `users.dat`.
3. Ask you to log in.
4. Create the genesis block (index 0), signed by the logged-in user, if
   `chain.dat` doesn't exist yet.

## Project Layout

```
Formative Assignment 1/
├── books.txt            # book registry (book_id,title,author)
├── members.txt          # member registry (member_id,full_name,course_code)
├── Makefile
├── README.md
├── src/
│   ├── main.c            # CLI menu, access control + entry point
│   ├── registry.h/.c     # Book/Member registry loading & lookup
│   ├── auth.h/.c         # accounts, PBKDF2 password hashing, login, roles
│   ├── crypto.h/.c       # SHA-256, per-user ECDSA keys, sign/verify (OpenSSL)
│   ├── blockchain.h/.c   # Block struct, chain ops, validation, persistence
│   └── input.h/.c        # safe line input, hidden password entry, int parsing
└── docs/                 # system design diagram, demo video script
```

Generated at runtime (not committed): `library_tracker`, `chain.dat`,
`users.dat`, `keys/`.

## CLI Usage

```
===== Blockchain Library Lending Tracker =====
Logged in as: admin (ADMIN)
1. Borrow Book
2. Return Book
3. Mark Book Overdue
4. View Lending Records
5. Validate Chain
6. Tamper With a Block (demo) [ADMIN]
7. Add User Account [ADMIN]
8. List Books
9. List Members
10. Exit
```

1. **Borrow Book**: enter a Book ID and Member ID (case-insensitive). Both
   are checked against the registries. An unknown ID prints
   `ERROR: Book or Member not found`, and a book already on loan is
   rejected. Otherwise a `BORROWED` block is created, signed, hashed and
   appended.
2. **Return Book**: enter the Book ID and Member ID. Both must exist, the
   book must have an active loan (its most recent `BORROWED` entry has no
   later `RETURNED`), and the member must be the borrower. Otherwise an error
   is printed. On success a `RETURNED` block is appended.
3. **Mark Book Overdue**: for a book currently on loan, shows how many days
   it has been out (loan period: 14 days) and appends an `OVERDUE` block
   for its borrower. The loan stays open until the book is returned.
4. **View Lending Records**: prints every block: book, title, member, name,
   action, who signed it, whether the signature verifies, timestamp and a
   truncated hash.
5. **Validate Chain**: recomputes every block's hash, checks `previous_hash`
   linkage (64 zeros for genesis), and verifies every signature with the
   signer's public key. It reports the first block that fails.
6. **Tamper With a Block (demo)** *(ADMIN only)*: overwrites a block's
   `action` field in memory *without* recomputing its hash, simulating an
   attacker editing the ledger. Option 5 then shows validation failing. While
   the chain is invalid, all new blocks are refused, so the tampered data is
   never saved. Restart the program to reload the genuine chain.
7. **Add User Account** *(ADMIN only)*: creates a new `LIBRARIAN` or `ADMIN`
   account with its own key pair.
8. **List Books**: shows every book in the registry with its ID, title,
   author and current status (`Available`, `On loan: <name> (<id>)` or
   `OVERDUE: <name> (<id>)`), worked out from the chain.
9. **List Members**: shows every member with their ID, name, course and the
   IDs of any books they currently have on loan.

## How Hashing & Signing Work

For each block:

1. The core fields (`index`, `timestamp`, `book_id`, `book_title`,
   `member_id`, `member_name`, `action`, `recorded_by`, `previous_hash`) are
   concatenated into a canonical buffer and hashed with SHA-256. This "data
   hash" is **signed** with the ECDSA private key of the logged-in user
   (`recorded_by`).
2. The signature bytes are appended to the same buffer, and the whole thing
   is hashed again with SHA-256 to produce the block's final `hash`, so the
   stored hash commits to every field, including the signature.
3. `previous_hash` is copied from the prior block's `hash`, linking the
   chain.

Validation re-derives both hashes and verifies each signature with the
**public key** in `keys/<recorded_by>_public.pem`, so any change to any
field, and any signature not made by that user's private key, is detected.

## Security Notes & Limitations

- Passwords are never stored. Only a random 16-byte salt and a
  PBKDF2-HMAC-SHA256 hash are kept in `users.dat` (mode `0600`). Password
  comparison is constant-time, and unknown usernames take as long to reject
  as wrong passwords.
- Private keys are encrypted at rest with the owner's password, so someone
  who copies `keys/` still cannot sign blocks without the password.
- This is a single-machine demo. An attacker with full write access to the
  folder could replace both a user's public key and `chain.dat` together. A
  production system would anchor public keys with a certificate authority
  and replicate the chain across independent nodes.
