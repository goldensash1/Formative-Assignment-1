#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "registry.h"

/* Splits a CSV line "a,b,c" (no quoted fields, matches the simple
 * books.txt / members.txt format described in the assignment). */
static int split_csv(char *line, char *fields[], int max_fields) {
    int count = 0;
    char *tok = strtok(line, ",\r\n");
    while (tok != NULL && count < max_fields) {
        fields[count++] = tok;
        tok = strtok(NULL, ",\r\n");
    }
    return count;
}

static int is_blank_line(const char *line) {
    for (const char *p = line; *p; p++) {
        if (!isspace((unsigned char)*p)) return 0;
    }
    return 1;
}

/* Trims leading/trailing whitespace in place and returns the new start. */
static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
    return s;
}

/* IDs are matched case-insensitively: store them upper-cased, the same way
 * user input is normalised in main.c. */
static void to_upper(char *s) {
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

static void copy_field(char *dst, size_t dst_size, const char *src) {
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

/* Reads `path` line by line, calling `add` with 3 trimmed fields for every
 * well-formed line. Returns the number of records added, or -1 if the file
 * could not be opened. */
static int load_csv3(const char *path, const char *what,
                     int (*add)(void *ctx, char *f[3], int line_no), void *ctx) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        printf("ERROR: Could not open %s registry file '%s'\n", what, path);
        return -1;
    }

    char line[256];
    int line_no = 0, added = 0;
    while (fgets(line, sizeof(line), fp)) {
        line_no++;
        if (is_blank_line(line)) continue;

        char line_copy[256];
        copy_field(line_copy, sizeof(line_copy), line);

        char *fields[3];
        if (split_csv(line_copy, fields, 3) < 3) {
            printf("WARNING: Skipping malformed line %d in %s\n", line_no, path);
            continue;
        }
        for (int i = 0; i < 3; i++) fields[i] = trim(fields[i]);
        to_upper(fields[0]);

        if (fields[0][0] == '\0') {
            printf("WARNING: Skipping line %d in %s (empty ID)\n", line_no, path);
            continue;
        }
        added += add(ctx, fields, line_no);
    }
    fclose(fp);
    return added;
}

static int add_book(void *ctx, char *f[3], int line_no) {
    Registry *reg = ctx;
    if (reg->book_count >= MAX_BOOKS) {
        printf("WARNING: Book registry full (max %d), ignoring line %d\n", MAX_BOOKS, line_no);
        return 0;
    }
    if (registry_find_book(reg, f[0])) {
        printf("WARNING: Duplicate book ID '%s' on line %d ignored\n", f[0], line_no);
        return 0;
    }
    Book *b = &reg->books[reg->book_count++];
    copy_field(b->book_id, sizeof(b->book_id), f[0]);
    copy_field(b->title, sizeof(b->title), f[1]);
    copy_field(b->author, sizeof(b->author), f[2]);
    return 1;
}

static int add_member(void *ctx, char *f[3], int line_no) {
    Registry *reg = ctx;
    if (reg->member_count >= MAX_MEMBERS) {
        printf("WARNING: Member registry full (max %d), ignoring line %d\n", MAX_MEMBERS, line_no);
        return 0;
    }
    if (registry_find_member(reg, f[0])) {
        printf("WARNING: Duplicate member ID '%s' on line %d ignored\n", f[0], line_no);
        return 0;
    }
    Member *m = &reg->members[reg->member_count++];
    copy_field(m->member_id, sizeof(m->member_id), f[0]);
    copy_field(m->full_name, sizeof(m->full_name), f[1]);
    copy_field(m->course_code, sizeof(m->course_code), f[2]);
    return 1;
}

int registry_load(Registry *reg, const char *books_path, const char *members_path) {
    reg->book_count = 0;
    reg->member_count = 0;

    int books = load_csv3(books_path, "book", add_book, reg);
    if (books == 0) printf("ERROR: Book registry '%s' is empty\n", books_path);

    int members = load_csv3(members_path, "member", add_member, reg);
    if (members == 0) printf("ERROR: Member registry '%s' is empty\n", members_path);

    return books > 0 && members > 0;
}

Book *registry_find_book(Registry *reg, const char *book_id) {
    for (int i = 0; i < reg->book_count; i++) {
        if (strcmp(reg->books[i].book_id, book_id) == 0) return &reg->books[i];
    }
    return NULL;
}

Member *registry_find_member(Registry *reg, const char *member_id) {
    for (int i = 0; i < reg->member_count; i++) {
        if (strcmp(reg->members[i].member_id, member_id) == 0) return &reg->members[i];
    }
    return NULL;
}
