
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/book.h"
#include "../include/db.h"

int addBook(Book **head, const Book *input)
{
    MYSQL *conn = getConnection();

    if (conn == NULL)
    {
        printf("Database is not connected!\n");
        return 0;
    }

    Book *newBook = malloc(sizeof(Book));

    if (newBook == NULL)
    {
        printf("Memory allocation failed!\n");
        return 0;
    }

    *newBook = *input;
    newBook->next = NULL;

    char title[sizeof(input->title) * 2 + 1];
    char author[sizeof(input->author) * 2 + 1];
    char isbn[sizeof(input->isbn) * 2 + 1];
    char publisher[sizeof(input->publisher) * 2 + 1];
    char category[sizeof(input->category) * 2 + 1];
    char query[2048];

    mysql_real_escape_string(conn, title, input->title,
                             (unsigned long)strlen(input->title));
    mysql_real_escape_string(conn, author, input->author,
                             (unsigned long)strlen(input->author));
    mysql_real_escape_string(conn, isbn, input->isbn,
                             (unsigned long)strlen(input->isbn));
    mysql_real_escape_string(conn, publisher, input->publisher,
                             (unsigned long)strlen(input->publisher));
    mysql_real_escape_string(conn, category, input->category,
                             (unsigned long)strlen(input->category));

    snprintf(query, sizeof(query),
        "INSERT INTO BOOKS "
        "(bookID, title, author, isbn, publisher, category, "
        "publicationYear, totalCopies, availableCopies, "
        "damagedCopies, lostCopies, active) "
        "VALUES (%d, '%s', '%s', '%s', '%s', '%s', %d, %d, %d, %d, %d, %d)",
        input->bookID, title, author, isbn, publisher, category,
        input->publicationYear, input->totalCopies,
        input->availableCopies, input->damagedCopies,
        input->lostCopies, input->active);

    if (mysql_query(conn, query) != 0)
    {
        printf("Could not add book: %s\n", mysql_error(conn));
        free(newBook);
        return 0;
    }

    if (*head == NULL)
    {
        *head = newBook;
    }
    else
    {
        Book *temp = *head;

        while (temp->next != NULL)
            temp = temp->next;

        temp->next = newBook;
    }

    printf("Book added to database successfully!\n");
    return 1;
}


Book *findBookByID(Book *head, int bookID)
{
    (void)head;  // Suppress unused parameter warning

    MYSQL *conn = getConnection();

    if (conn == NULL)
    {
        printf("Database is not connected!\n");
        return NULL;
    }

    char query[256];

    snprintf(query, sizeof(query),
             "SELECT bookID, title, author, isbn, publisher, "
             "category, publicationYear, totalCopies, "
             "availableCopies, damagedCopies, lostCopies, active "
             "FROM BOOKS WHERE bookID = %d",
             bookID);

    if (mysql_query(conn, query) != 0)
    {
        printf("Search failed: %s\n", mysql_error(conn));
        return NULL;
    }

    MYSQL_RES *result = mysql_store_result(conn);

    if (result == NULL)
    {
        printf("Could not retrieve results: %s\n",
               mysql_error(conn));
        return NULL;
    }

    MYSQL_ROW row = mysql_fetch_row(result);

    if (row == NULL)
    {
        mysql_free_result(result);
        return NULL;
    }

    Book *found = malloc(sizeof(Book));

    if (found == NULL)
    {
        printf("Memory allocation failed!\n");
        mysql_free_result(result);
        return NULL;
    }

    found->bookID = atoi(row[0]);
    snprintf(found->title, sizeof(found->title), "%s",
             row[1] ? row[1] : "");
    snprintf(found->author, sizeof(found->author), "%s",
             row[2] ? row[2] : "");
    snprintf(found->isbn, sizeof(found->isbn), "%s",
             row[3] ? row[3] : "");
    snprintf(found->publisher, sizeof(found->publisher), "%s",
             row[4] ? row[4] : "");
    snprintf(found->category, sizeof(found->category), "%s",
             row[5] ? row[5] : "");

    found->publicationYear = atoi(row[6]);
    found->totalCopies = atoi(row[7]);
    found->availableCopies = atoi(row[8]);
    found->damagedCopies = atoi(row[9]);
    found->lostCopies = atoi(row[10]);
    found->active = atoi(row[11]);
    found->next = NULL;

    mysql_free_result(result);

    return found;
}



Book *linearSearchByTitle(Book *head, const char *title)
{
    (void)head;

    MYSQL *conn = getConnection();

    if (conn == NULL)
    {
        printf("Database is not connected!\n");
        return NULL;
    }

    char escapedTitle[400];
    char query[600];

    mysql_real_escape_string(conn, escapedTitle, title, strlen(title));

    snprintf(query, sizeof(query),
             "SELECT bookID, title, author, isbn, publisher, category, "
             "publicationYear, totalCopies, availableCopies, "
             "damagedCopies, lostCopies, active "
             "FROM BOOKS WHERE title LIKE '%%%s%%' LIMIT 1",
             escapedTitle);

    if (mysql_query(conn, query) != 0)
    {
        printf("Search failed: %s\n", mysql_error(conn));
        return NULL;
    }

    MYSQL_RES *result = mysql_store_result(conn);

    if (result == NULL)
    {
        printf("Could not retrieve results: %s\n", mysql_error(conn));
        return NULL;
    }

    MYSQL_ROW row = mysql_fetch_row(result);

    if (row == NULL)
    {
        printf("No book found matching \"%s\".\n", title);
        mysql_free_result(result);
        return NULL;
    }

    Book *found = calloc(1, sizeof(Book));

    if (found == NULL)
    {
        printf("Memory allocation failed!\n");
        mysql_free_result(result);
        return NULL;
    }

    found->bookID = atoi(row[0]);
    snprintf(found->title, sizeof(found->title), "%s", row[1] ? row[1] : "");
    snprintf(found->author, sizeof(found->author), "%s", row[2] ? row[2] : "");
    snprintf(found->isbn, sizeof(found->isbn), "%s", row[3] ? row[3] : "");
    snprintf(found->publisher, sizeof(found->publisher), "%s", row[4] ? row[4] : "");
    snprintf(found->category, sizeof(found->category), "%s", row[5] ? row[5] : "");

    found->publicationYear = row[6] ? atoi(row[6]) : 0;
    found->totalCopies = row[7] ? atoi(row[7]) : 0;
    found->availableCopies = row[8] ? atoi(row[8]) : 0;
    found->damagedCopies = row[9] ? atoi(row[9]) : 0;
    found->lostCopies = row[10] ? atoi(row[10]) : 0;
    found->active = row[11] ? atoi(row[11]) : 0;

    mysql_free_result(result);
    return found;
}

void displayBooks(Book *head)
{
    (void)head;

    MYSQL *conn = getConnection();

    if (conn == NULL)
    {
        printf("Database is not connected!\n");
        return;
    }

    if (mysql_query(conn,
        "SELECT bookID, title, author, isbn, availableCopies "
        "FROM BOOKS ORDER BY bookID") != 0)
    {
        printf("Could not retrieve books: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *result = mysql_store_result(conn);

    if (result == NULL)
    {
        printf("Could not read books: %s\n", mysql_error(conn));
        return;
    }

    if (mysql_num_rows(result) == 0)
    {
        printf("No books available.\n");
        mysql_free_result(result);
        return;
    }

    MYSQL_ROW row;

    while ((row = mysql_fetch_row(result)) != NULL)
    {
        printf("\nBook ID: %s\n", row[0]);
        printf("Title: %s\n", row[1] ? row[1] : "");
        printf("Author: %s\n", row[2] ? row[2] : "");
        printf("ISBN: %s\n", row[3] ? row[3] : "");
        printf("Available Copies: %s\n", row[4]);
        printf("-------------------------\n");
    }

    mysql_free_result(result);
}


int updateBookTitle(Book *head, int bookID, const char *newTitle)
{
    (void)head;

    MYSQL *conn = getConnection();

    if (conn == NULL)
    {
        printf("Database is not connected!\n");
        return 0;
    }

    char escapedTitle[201];
    char query[300];

    mysql_real_escape_string(
        conn,
        escapedTitle,
        newTitle,
        (unsigned long)strlen(newTitle)
    );

    snprintf(
        query,
        sizeof(query),
        "UPDATE BOOKS SET title='%s' WHERE bookID=%d",
        escapedTitle,
        bookID
    );

    if (mysql_query(conn, query) != 0)
    {
        printf("Could not update book: %s\n", mysql_error(conn));
        return 0;
    }

    my_ulonglong affected = mysql_affected_rows(conn);

    if (affected == (my_ulonglong)-1)
    {
        printf("Could not determine update result.\n");
        return 0;
    }

    if (affected == 0)
    {
        printf("Book not found or title unchanged.\n");
        return 0;
    }

    printf("Book title updated successfully!\n");
    return 1;
}

int deactivateBook(Book *head, int bookID)
{
    (void)head;

    MYSQL *conn = getConnection();

    if (conn == NULL)
    {
        printf("Database is not connected!\n");
        return 0;
    }

    char query[150];

    snprintf(query, sizeof(query),
             "UPDATE BOOKS SET active = 0 WHERE bookID = %d",
             bookID);

    if (mysql_query(conn, query) != 0)
    {
        printf("Could not deactivate book: %s\n",
               mysql_error(conn));
        return 0;
    }

    if (mysql_affected_rows(conn) == 0)
    {
        printf("Book not found or already inactive.\n");
        return 0;
    }

    printf("Book deactivated successfully!\n");
    return 1;
}