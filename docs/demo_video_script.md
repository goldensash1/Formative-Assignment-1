# Demo Video Script (target: about 4:30)

The rubric asks the video to show: **registry loading, lending actions, invalid ID
handling, chain validation and tamper detection**, with a clear, professional
explanation. This script covers all five, plus the security features
(login, roles, per-user keys) that the "Security and Use of Cryptographic Keys"
criterion rewards.

---

## Before you hit record

1. Terminal font size 16–18pt, window wide enough for the records table
   (about 120 columns). Hide unrelated tabs and notifications.
2. Have the code editor open on `src/blockchain.h` (Block struct) in a second
   window.
3. Reset to a fresh state so the first-run setup appears on camera:
   ```bash
   make distclean && make
   ```
4. Pick two passwords now (at least 8 characters) for `admin` and
   `librarian1`. Typing is hidden, so nothing sensitive shows on screen.
5. Practise once without recording, then run `make distclean && make` again.

---

## Scene 1: Introduction (0:00 – 0:25)

**Show:** the README or the system design diagram (`docs/system_design.svg`).

> "Hi, I'm [name]. This is my Blockchain-Based Library Book Lending Tracker,
> written in C with OpenSSL. Paper lending logs can be quietly edited. Here,
> every borrow, return and overdue event is a block that's hashed with SHA-256,
> linked to the previous block, and digitally signed with ECDSA by the
> librarian who recorded it, so any change to the history is detectable."

Point at the diagram: registries → CLI → blockchain, and the auth/crypto
modules on the right.

## Scene 2: Code structure and block (0:25 – 0:55)

**Show:** `src/blockchain.h`, the `Block` struct.

> "Each block holds the fields from the specification: index, timestamp,
> book ID and title, member ID and name, the action, the previous hash, the
> ECDSA signature and the block's own SHA-256 hash. I've added `recorded_by`,
> the authenticated librarian who signed it. The genesis block's previous
> hash is 64 zeros."

Briefly show `books.txt` and `members.txt`.

## Scene 3: Registry loading and error handling (0:55 – 1:25)

**Do:** show the missing-file error first.

```bash
mv members.txt members.bak
./library_tracker
```

> "Before anything else, the program loads both registries into arrays of
> structs. If a file is missing, or empty, it stops with an error."

```bash
mv members.bak members.txt
./library_tracker
```

> "With both files present it loads 5 books and 5 members."

## Scene 4: First-run setup and login (1:25 – 2:00)

**Do:** create the admin account (username `admin`), then log in.

> "On first run there are no accounts, so it asks me to create an
> administrator. Passwords are never stored. Only a random salt and a
> PBKDF2-SHA256 hash go into `users.dat`. It also generates this user's own
> ECDSA key pair. The private key is encrypted with the password, so only
> this user can sign blocks."

After logging in:

> "Logging in unlocks my private key. The genesis block is created and signed,
> and the chain is verified at startup."

## Scene 5: Borrowing: valid and invalid IDs (2:00 – 2:40)

**Do**, in order:

| Option | Book ID | Member ID | Expected output |
|---|---|---|---|
| 1 | `BK001` | `ALU001` | SUCCESS, Block #1 |
| 1 | `BK999` | `ALU001` | `ERROR: Book or Member not found` |
| 1 | `BK002` | `ALU999` | `ERROR: Book or Member not found` |
| 1 | `BK001` | `ALU002` | already on loan |
| 1 | `BK002` | `ALU002` | SUCCESS, Block #2 |

> "A valid borrow creates, signs and appends a block. An unknown book or
> member ID is rejected before anything is recorded. And a book that's
> already out can't be borrowed again."

## Scene 6: Overdue and return (2:40 – 3:05)

**Do:**

| Option | Input | Expected |
|---|---|---|
| 3 | `BK002` | OVERDUE block for Jane Smith |
| 2 | `BK001` / `ALU002` | error: on loan to John Doe, not Jane Smith |
| 2 | `BK001` / `ALU001` | SUCCESS, RETURNED block |
| 2 | `BK001` / `ALU001` | error: no active loan (already returned) |

> "A return only works for the member who actually borrowed the book, and a
> book can't be returned twice."

## Scene 7: Roles and a second signer (3:05 – 3:30)

**Do:** option 7 → role `1` → create `librarian1`. Exit (8), run again, log in as
`librarian1`. Try option 6 to show *Access denied*. Borrow `BK003` / `ALU003`.

> "Admins can add accounts. This librarian has their own key pair and cannot
> use admin functions like the tamper demo. Their block is signed with their
> own key."

## Scene 8: View records and validate (3:30 – 3:50)

**Do:** option 4, then option 5.

> "The records show each block's title, member, action, who signed it,
> whether the signature is valid, and the timestamp. Validation recomputes
> every hash, checks every previous-hash link, and verifies each signature
> with the signer's public key. The chain is valid."

## Scene 9: Tamper detection (3:50 – 4:25)

**Do:** exit, log in as `admin`. Option 6 → block `1` → `RETURNED`. Then option
5, then option 4, then try option 1 (`BK004` / `ALU004`).

> "Now I'll simulate a dishonest edit: I change block 1 from BORROWED to
> RETURNED without recomputing its hash. Validation immediately reports that
> block 1's hash no longer matches, and the records show its signature is now
> INVALID. The system also refuses to add any new blocks on top of a
> tampered chain, so the fake edit can never be saved."

**Optional (strong, about 15s):** tamper with the file on disk. Exit, then:

```bash
python3 -c "d=bytearray(open('chain.dat','rb').read()); i=d.find(b'Jane Smith'); d[i:i+10]=b'Evil Actor'; open('chain.dat','wb').write(d)"
./library_tracker
```

> "Even if someone edits `chain.dat` directly, the startup check catches it."

## Scene 10: Wrap-up (4:25 – 4:40)

> "To summarise: registries validate every ID, SHA-256 links the blocks,
> per-librarian ECDSA keys authenticate each action, and validation detects
> any tampering. Thanks for watching."

---

## Checklist before uploading

- [ ] Registry loaded (and missing-file error shown)
- [ ] Valid borrow and return recorded
- [ ] Invalid book ID **and** invalid member ID rejected
- [ ] Records viewed with signature validity
- [ ] Chain validated as VALID
- [ ] Tamper → validation reports INVALID
- [ ] Length 3–5 minutes, audio clear, terminal readable
