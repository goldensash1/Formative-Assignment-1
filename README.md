# Blockchain-Based Library Book Lending Tracker

A C implementation of an immutable, cryptographically verifiable library
lending ledger. Every borrow/return event is recorded as a block in a
hash-linked, digitally-signed chain, so a librarian or borrower cannot
quietly rewrite history.

## Features

- **Book & Member registries** loaded from `books.txt` / `members.txt` at
  startup, used to validate every lending action.
- **Blockchain** of lending records, linked by SHA-256 hashes.
- **ECDSA (P-256) digital signatures** on every block, generated with
  OpenSSL's EVP API.
- **Chain validation** that detects hash tampering, broken links, and
  invalid signatures.
- **File-based persistence** (`chain.dat`) so the chain survives restarts.
- **CLI** to borrow, return, view records, validate the chain, and run a
  tamper-detection demo.

## Required Libraries / Dependencies

- A C compiler (`gcc` or `clang`)
- **OpenSSL** (development headers + libcrypto/libssl), used for SHA-256
  hashing and ECDSA signing/verification.
  - macOS: `brew install openssl@3`
  - Debian/Ubuntu: `sudo apt-get install libssl-dev`

### Apple Silicon (M1/M2/M3/...) note

If your only installed Homebrew OpenSSL is the Intel (x86_64) build (e.g.
installed under `/usr/local` via a Rosetta shell), the provided `Makefile`
automatically detects this and compiles the program as an x86_64 binary,
which runs fine under Rosetta 2. If Rosetta isn't installed yet, install it
once with:

```bash
softwareupdate --install-rosetta --agree-to-license
```

If you instead have a native arm64 OpenSSL (e.g. `arch -arm64 brew install
openssl@3` under `/opt/homebrew`), the Makefile will pick that up and build
a native arm64 binary automatically — no changes needed.

## Build & Run

```bash
make        # compiles ./library_tracker
make run    # compiles (if needed) and runs it
make clean  # removes the binary and chain.dat
```

Or manually:

```bash
gcc -Wall -Wextra -std=c11 -I$(brew --prefix openssl@3)/include \
    -o library_tracker src/main.c src/blockchain.c src/registry.c src/crypto.c \
    -L$(brew --prefix openssl@3)/lib -lssl -lcrypto
./library_tracker
```

On first run the program will:

1. Load `books.txt` and `members.txt` into memory.
2. Generate a new ECDSA key pair (`ec_private.pem`, `ec_public.pem`) if one
   doesn't already exist.
3. Create the genesis block (index 0) if `chain.dat` doesn't already exist.

## Project Layout

```
Formative Assignment 1/
├── books.txt            # book registry (book_id,title,author)
├── members.txt          # member registry (member_id,full_name,course_code)
├── Makefile
├── README.md
├── src/
│   ├── main.c            # CLI menu + entry point
│   ├── registry.h/.c     # Book/Member registry loading & lookup
│   ├── crypto.h/.c       # SHA-256 hashing + ECDSA sign/verify (OpenSSL)
│   └── blockchain.h/.c   # Block struct, chain ops, hashing, persistence
└── docs/                 # system design diagram, report assets
```

Generated at runtime (not committed): `library_tracker`, `chain.dat`,
`ec_private.pem`, `ec_public.pem`.

## CLI Usage

```
===== Blockchain Library Lending Tracker =====
1. Borrow Book
2. Return Book
3. View Lending Records
4. Validate Chain
5. Tamper With a Block (demo)
6. Exit
```

1. **Borrow Book** — enter a Book ID and Member ID. Both are checked
   against the registries; an unknown ID (or a book already on loan)
   aborts with an error. Otherwise a new `BORROWED` block is created,
   signed, hashed, and appended to the chain.
2. **Return Book** — enter a Book ID. The chain is scanned for that book's
   most recent unreturned `BORROWED` block; if none exists, an error is
   printed. Otherwise a `RETURNED` block is appended.
3. **View Lending Records** — prints every block (title, member, action,
   timestamp, truncated hash, and whether its signature currently
   verifies).
4. **Validate Chain** — recomputes every block's hash, checks
   `previous_hash` linkage, and re-verifies every signature, reporting the
   first block where something doesn't match.
5. **Tamper With a Block (demo)** — deliberately overwrites a block's
   `action` field in memory *without* recomputing its hash (simulating an
   attacker editing the ledger directly), so option 4 can be used
   immediately afterward to show validation failing. This change is not
   saved to `chain.dat`, so restarting the program restores the genuine
   chain.

## How Hashing & Signing Work

For each block:

1. The core fields (`index`, `timestamp`, `book_id`, `book_title`,
   `member_id`, `member_name`, `action`, `previous_hash`) are concatenated
   into a canonical buffer and hashed with SHA-256 — this is the
   "data hash" that gets **signed** with the ECDSA private key.
2. The resulting signature bytes are appended to the same buffer, and the
   whole thing is hashed again with SHA-256 to produce the block's final
   `hash` field — so the stored hash commits to every field, including the
   signature.
3. `previous_hash` is copied from the prior block's `hash`, linking the
   chain.

Validation re-derives both hashes and re-verifies the signature with the
stored public key, so any change to any field (including a forged
signature) is detectable.
