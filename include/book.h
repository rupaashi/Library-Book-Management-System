
#ifndef BOOK_H
#define BOOK_H

#include "common.h"

int addBook(Book **head, const Book *input);
Book *findBookByID(Book *head, int bookID);
void displayBooks(Book *head);
int updateBookTitle(Book *head, int bookID,
                    const char *newTitle);
int deactivateBook(Book *head, int bookID);

#endif