#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
        if (*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') return 0;
    }
    return 1;
}

static int load_books(Registry *reg, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        printf("ERROR: Could not open book registry file '%s'\n", path);
        return 0;
    }

    char line[256];
    reg->book_count = 0;
    while (fgets(line, sizeof(line), fp) && reg->book_count < MAX_BOOKS) {
        if (is_blank_line(line)) continue;
        char line_copy[256];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';

        char *fields[3];
        int n = split_csv(line_copy, fields, 3);
        if (n < 3) {
            printf("WARNING: Skipping malformed line in %s: %s", path, line);
            continue;
        }

        Book *b = &reg->books[reg->book_count];
        strncpy(b->book_id, fields[0], sizeof(b->book_id) - 1);
        b->book_id[sizeof(b->book_id) - 1] = '\0';
        strncpy(b->title, fields[1], sizeof(b->title) - 1);
        b->title[sizeof(b->title) - 1] = '\0';
        strncpy(b->author, fields[2], sizeof(b->author) - 1);
        b->author[sizeof(b->author) - 1] = '\0';
        reg->book_count++;
    }
    fclose(fp);

    if (reg->book_count == 0) {
        printf("ERROR: Book registry '%s' is empty\n", path);
        return 0;
    }
    return 1;
}

static int load_members(Registry *reg, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        printf("ERROR: Could not open member registry file '%s'\n", path);
        return 0;
    }

    char line[256];
    reg->member_count = 0;
    while (fgets(line, sizeof(line), fp) && reg->member_count < MAX_MEMBERS) {
        if (is_blank_line(line)) continue;
        char line_copy[256];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';

        char *fields[3];
        int n = split_csv(line_copy, fields, 3);
        if (n < 3) {
            printf("WARNING: Skipping malformed line in %s: %s", path, line);
            continue;
        }

        Member *m = &reg->members[reg->member_count];
        strncpy(m->member_id, fields[0], sizeof(m->member_id) - 1);
        m->member_id[sizeof(m->member_id) - 1] = '\0';
        strncpy(m->full_name, fields[1], sizeof(m->full_name) - 1);
        m->full_name[sizeof(m->full_name) - 1] = '\0';
        strncpy(m->course_code, fields[2], sizeof(m->course_code) - 1);
        m->course_code[sizeof(m->course_code) - 1] = '\0';
        reg->member_count++;
    }
    fclose(fp);

    if (reg->member_count == 0) {
        printf("ERROR: Member registry '%s' is empty\n", path);
        return 0;
    }
    return 1;
}

int registry_load(Registry *reg, const char *books_path, const char *members_path) {
    int books_ok = load_books(reg, books_path);
    int members_ok = load_members(reg, members_path);
    return books_ok && members_ok;
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
