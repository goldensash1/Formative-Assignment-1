#ifndef REGISTRY_H
#define REGISTRY_H

#define MAX_BOOKS 200
#define MAX_MEMBERS 200

typedef struct {
    char book_id[20];
    char title[80];
    char author[50];
} Book;

typedef struct {
    char member_id[20];
    char full_name[50];
    char course_code[10];
} Member;

typedef struct {
    Book books[MAX_BOOKS];
    int book_count;
    Member members[MAX_MEMBERS];
    int member_count;
} Registry;

/* Loads books.txt and members.txt into the registry.
 * Returns 1 on success, 0 if either file is missing or empty. */
int registry_load(Registry *reg, const char *books_path, const char *members_path);

/* Returns pointer to the Book with the given id, or NULL if not found. */
Book *registry_find_book(Registry *reg, const char *book_id);

/* Returns pointer to the Member with the given id, or NULL if not found. */
Member *registry_find_member(Registry *reg, const char *member_id);

#endif
