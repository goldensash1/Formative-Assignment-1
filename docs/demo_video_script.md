# Demo Video Script

Blockchain-Based Library Book Lending Tracker

Length: about 4 to 5 minutes. The "Say" parts are what you speak. The "Do" parts are what you type or show on screen.

## Before recording

- Make the terminal font bigger (16 to 18) and the window wide enough for the records table.
- Open `src/blockchain.h` in your code editor.
- Reset everything so the first-run setup shows on camera:

```bash
make distclean && make
```

- Decide your two passwords now, one for `admin` and one for `librarian1`. They are hidden when you type them.
- Do one practice run, then run `make distclean && make` again before you record.

---

## Part 1: Introduction (about 30 seconds)

Do: show the system design diagram (`docs/system_design.svg`) or the GitHub page.

Say:

"Hello, my name is Golden Sash Munyankindi, and this is my demo of the Blockchain-Based Library Book Lending Tracker. It is written in C and uses OpenSSL.

The problem with normal lending records is that anyone with access can change them. For example, a librarian could mark a lost book as returned. In my program every borrow, return and overdue action is saved as a block. Each block is hashed with SHA-256, points to the hash of the block before it, and is signed by the librarian who recorded it. If an old record is changed, the program can detect it."

## Part 2: The block and the registry files (about 30 seconds)

Do: show the `Block` struct in `src/blockchain.h`, then `books.txt` and `members.txt`.

Say:

"This is the block structure. It has the index, timestamp, book ID and title, member ID and name, the action, the previous hash, the signature and the block's own hash. I also added recorded_by, which is the username of the librarian who signed the block, and sig_len, the length of the signature.

These are the two registry files. books.txt has the book ID, title and author, and members.txt has the member ID, name and course code. The program only accepts IDs that are in these files."

## Part 3: Loading the registries (about 30 seconds)

Do:

```bash
mv members.txt members.bak
./library_tracker
```

Say:

"First I'll show what happens if a registry file is missing. I've renamed members.txt, and the program prints an error and stops, because it can't check any lending action without it."

Do:

```bash
mv members.bak members.txt
./library_tracker
```

Say:

"Now with both files back, it loads 5 books and 5 members into memory."

## Part 4: First run and login (about 40 seconds)

Do: create the admin account. Username `admin`, then type the password twice. Then log in with the same details.

Say:

"Because this is the first run, there are no accounts yet, so it asks me to create an administrator. The password is not saved. Only a salted hash of it is saved, in users.dat. The program also creates a key pair for this user. The private key is encrypted with the password, so nobody else can use it to sign blocks.

Now I log in. Logging in unlocks my private key. Since there is no chain yet, the program creates the genesis block, which has a previous hash of 64 zeros. Then it checks the chain, and it's valid."

## Part 5: Borrowing with valid and invalid IDs (about 45 seconds)

Do, one at a time:

| Menu option | Book ID | Member ID |
|---|---|---|
| 1 | BK001 | ALU001 |
| 1 | BK999 | ALU001 |
| 1 | BK002 | ALU999 |
| 1 | BK001 | ALU002 |
| 1 | BK002 | ALU002 |

Say:

"I'll borrow book BK001 for member ALU001. Both IDs are valid, so a new block is created, signed and added to the chain.

Now I'll try book BK999, which doesn't exist. The program says Book or Member not found, and nothing is recorded. Same with member ALU999, which is not in the registry.

If I try to borrow BK001 again, it's refused because the book is already on loan.

And one more valid borrow: BK002 for Jane Smith."

## Part 6: Overdue and return (about 40 seconds)

Do, one at a time:

| Menu option | Book ID | Member ID |
|---|---|---|
| 3 | BK002 | (not asked) |
| 2 | BK001 | ALU002 |
| 2 | BK001 | ALU001 |
| 2 | BK001 | ALU001 |

Say:

"Option 3 marks a book as overdue. Americanah is still on loan to Jane Smith, so an OVERDUE block is added.

Now I'll return BK001, but using the wrong member, ALU002. The program refuses, because that book was borrowed by John Doe.

With the correct member it works, and a RETURNED block is added.

If I try to return the same book again, it says there's no active loan, because it was already returned."

## Part 7: A second user and access control (about 40 seconds)

Do: option 7, role `1`, username `librarian1`, password twice. Then option 10 to exit. Run `./library_tracker` again and log in as `librarian1`. Choose option 6. Then option 1 with `BK003` and `ALU003`. Then option 10.

Say:

"As the admin I can add a new librarian account. This librarian gets their own key pair.

Now I'll log in as the librarian. If I choose the tamper option, access is denied, because only an admin can do that.

The librarian can still borrow books. This block is signed with the librarian's own private key, not the admin's."

## Part 8: Book and member lists, records and validation (about 50 seconds)

Do: run `./library_tracker`, log in as `admin`, choose option 8, then option 9.

Say:

"Option 8 lists all the books with their IDs. The status comes from the chain. Three books are available, Americanah is overdue with Jane Smith, and The River Between is on loan to Amara Diallo.

Option 9 lists all the members with their IDs and course, and shows which books each of them has at the moment."

Do: choose option 4, then option 5.

Say:

"This is the list of all the lending records. For each block it shows the book, the member, the action, who signed it, whether the signature is valid, the time and the start of the hash. You can see the last block was signed by librarian1.

Option 5 validates the chain. It recalculates every hash, checks that each block points to the hash of the block before it, and checks every signature using the signer's public key. The chain is valid."

## Part 9: Tamper detection (about 45 seconds)

Do: option 6, block index `1`, new value `RETURNED`. Then option 5. Then option 4. Then option 1 with `BK004` and `ALU004`.

Say:

"Now I'll show tamper detection. Block 1 says John Doe borrowed Things Fall Apart. I'll change it to RETURNED without updating the hash, as if someone edited the record to hide that the book is missing.

When I validate, the program says block 1's stored hash doesn't match its data, so the chain is invalid. In the records, block 1's signature now shows INVALID.

If I try to borrow another book now, the program refuses to add a block, because the chain has been tampered with. That way the fake change can never be saved."

Optional, if you have time. Do: option 10 to exit, then:

```bash
python3 -c "d=bytearray(open('chain.dat','rb').read()); i=d.find(b'Jane Smith'); d[i:i+10]=b'Evil Actor'; open('chain.dat','wb').write(d)"
./library_tracker
```

Say:

"Even if someone edits the chain.dat file directly, the program finds the change as soon as it starts."

## Part 10: Closing (about 15 seconds)

Say:

"So to sum up: the registries make sure only real books and members are recorded, SHA-256 links the blocks together, each librarian signs their actions with their own key, and any change to an old record is detected. The code is on my GitHub. Thank you for watching."

---

## Checklist

- [ ] Registry loading shown, including the missing-file error
- [ ] Valid borrow and return recorded
- [ ] Invalid book ID and invalid member ID rejected
- [ ] Records viewed, with signature validity shown
- [ ] Chain validated
- [ ] Tampering detected
- [ ] Video is between 3 and 5 minutes, and the voice and text are clear
