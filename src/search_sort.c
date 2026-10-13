#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/search_sort.h"
#include "../include/db.h"

/* Merge two sorted linked lists */
static Book *merge(Book *left, Book *right)
{
    Book dummy;
    Book *tail = &dummy;

    dummy.next = NULL;

    while (left != NULL && right != NULL)
    {
        if (left->bookID <= right->bookID)
        {
            tail->next = left;
            left = left->next;
        }
        else
        {
            tail->next = right;
            right = right->next;
        }

        tail = tail->next;
    }

    if (left != NULL)
        tail->next = left;
    else
        tail->next = right;

    return dummy.next;
}

/* Find the middle of a linked list */
static Book *getMiddle(Book *head)
{
    Book *slow = head;
    Book *fast = head;

    while (fast->next != NULL && fast->next->next != NULL)
    {
        slow = slow->next;
        fast = fast->next->next;
    }

    return slow;
}

/* Merge Sort */
static Book *mergeSort(Book *head)
{
    if (head == NULL || head->next == NULL)
    {
        return head;
    }

    Book *middle = getMiddle(head);
    Book *right = middle->next;

    middle->next = NULL;

    Book *leftSorted = mergeSort(head);
    Book *rightSorted = mergeSort(right);

    return merge(leftSorted, rightSorted);
}


/* Sort books by ID using the database */

void sortBooksByID(Book **head)
{
    (void)head;

    MYSQL *conn = getConnection();

    if (conn == NULL)
    {
        printf("Database is not connected!\n");
        return;
    }

    if (mysql_query(conn,
        "SELECT bookID, title FROM BOOKS") != 0)
    {
        printf("Could not fetch books: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *result = mysql_store_result(conn);

    if (result == NULL)
    {
        printf("Could not retrieve books: %s\n", mysql_error(conn));
        return;
    }

    Book *tempHead = NULL;
    Book *tempTail = NULL;
    MYSQL_ROW row;

    while ((row = mysql_fetch_row(result)) != NULL)
    {
        Book *newNode = calloc(1, sizeof(Book));

        if (newNode == NULL)
        {
            printf("Memory allocation failed!\n");
            mysql_free_result(result);

            while (tempHead != NULL)
            {
                Book *temp = tempHead;
                tempHead = tempHead->next;
                free(temp);
            }

            return;
        }

        newNode->bookID = atoi(row[0]);
        snprintf(newNode->title, sizeof(newNode->title),
                 "%s", row[1] ? row[1] : "");

        if (tempHead == NULL)
        {
            tempHead = newNode;
            tempTail = newNode;
        }
        else
        {
            tempTail->next = newNode;
            tempTail = newNode;
        }
    }

    mysql_free_result(result);

    if (tempHead == NULL)
    {
        printf("No books available to sort.\n");
        return;
    }

    tempHead = mergeSort(tempHead);

    printf("\nBooks sorted by ID using Merge Sort:\n");
    printf("------------------------------------\n");

    Book *temp = tempHead;

    while (temp != NULL)
    {
        printf("Book ID: %d\n", temp->bookID);
        printf("Title: %s\n", temp->title);
        printf("------------------------------------\n");

        temp = temp->next;
    }

    while (tempHead != NULL)
    {
        Book *temp = tempHead;
        tempHead = tempHead->next;
        free(temp);
    }
}
/* Display books sorted by title */
void displayBooksByTitle(Book *head)
{
    Book *temp = head;

    while (temp != NULL)
    {
        printf("%s (ID: %d)\n", temp->title, temp->bookID);
        temp = temp->next;
    }
}

