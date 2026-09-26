#ifndef INPUT_H
#define INPUT_H

#include <stddef.h>

#define INPUT_OK        1
#define INPUT_EOF       0
#define INPUT_TOO_LONG -1

/* Prints `prompt`, then reads one line from stdin into `buf` (newline
 * stripped). Returns INPUT_OK, INPUT_EOF when stdin is closed, or
 * INPUT_TOO_LONG when the line did not fit (the rest of the line is
 * discarded so it cannot leak into the next prompt, and buf is emptied). */
int read_line(const char *prompt, char *buf, size_t size);

/* Same as read_line, but disables terminal echo while typing when stdin is
 * an interactive terminal. */
int read_password(const char *prompt, char *buf, size_t size);

/* Returns 1 once stdin has reached end-of-file. */
int input_eof(void);

/* Strictly parses a base-10 int (optional surrounding whitespace).
 * Returns 1 on success, 0 if the text is empty, non-numeric or out of range. */
int parse_int(const char *s, int *out);

/* Removes all whitespace and upper-cases the string in place (IDs are
 * case-insensitive, e.g. " bk001 " -> "BK001"). */
void to_upper_trim(char *s);

#endif
