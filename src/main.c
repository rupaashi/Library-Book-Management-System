#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/common.h"
#include "../include/book.h"
#include "../include/member.h"
#include "../include/search_sort.h"
#include "../include/circulation.h"

/* Ask for a whole number. Returns 1 on success, 0 on bad input
   (the bad input is discarded so the menu keeps working). */
static int readInt(const char *prompt, int *value)
{
    printf("%s", prompt);

    if (scanf("%d", value) != 1)
    {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF)
        {
        }

        printf("Invalid input. Please enter a number.\n");
        return 0;
    }

    return 1;
}

int main()
{
   if (!connectDB())
{
    return 1;
}

    Book *bookHead = NULL;
    Member *memberHead = NULL;

    int choice;
    int id;
    char title[100];

    do
    {
        printf("\n===== LIBRARY MANAGEMENT SYSTEM =====\n");
        printf("1. Add Book\n");
        printf("2. Display Books\n");
        printf("3. Search Book by ID\n");
        printf("4. Search Book by Title\n");
        printf("5. Sort Books by ID\n");
        printf("6. Add Member\n");
        printf("7. Display Members\n");
        printf("8. Search Member by ID\n");
        printf("9. Deactivate Book\n");
        printf("10. Deactivate Member\n");
        printf("11. Issue Book\n");
        printf("12. Return Book\n");
        printf("13. Renew Loan\n");
        printf("14. View Active Loans\n");
        printf("15. View Overdue Loans\n");
        printf("0. Exit\n");
        printf("Enter your choice: ");

        if (scanf("%d", &choice) != 1)
        {
            printf("Invalid input. Please enter a number.\n");

            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF)
            {
            }

            continue;
        }

        switch (choice)
        {
            case 1:
            {
                Book newBook = {0};

                printf("Enter Book ID: ");
                scanf("%d", &newBook.bookID);

                printf("Enter Title: ");
                scanf(" %99[^\n]", newBook.title);

                printf("Enter Author: ");
                scanf(" %99[^\n]", newBook.author);

                printf("Enter ISBN: ");
                scanf(" %19s", newBook.isbn);

                printf("Enter Publisher: ");
                scanf(" %99[^\n]", newBook.publisher);

                printf("Enter Category: ");
                scanf(" %49[^\n]", newBook.category);

                printf("Enter Publication Year: ");
                scanf("%d", &newBook.publicationYear);

                printf("Enter Total Copies: ");
                scanf("%d", &newBook.totalCopies);

                newBook.availableCopies = newBook.totalCopies;
                newBook.active = 1;

                addBook(&bookHead, &newBook);
                break;
            }

            case 2:
                displayBooks(bookHead);
                break;

           
case 3:
{
    printf("Enter Book ID to search: ");
    scanf("%d", &id);

    Book *found = findBookByID(bookHead, id);

    if (found != NULL)
    {
        printf("Book found: %s\n", found->title);
        free(found);
    }
    else
    {
        printf("Book not found.\n");
    }

    break;
}

           
case 4:
{
    printf("Enter Book Title to search: ");
    scanf(" %99[^\n]", title);
       Book *found = linearSearchByTitle(bookHead, title);

if (found != NULL)
{
    printf("Book ID: %d\n", found->bookID);
    printf("Title: %s\n", found->title);
    printf("Author: %s\n", found->author);
    free(found);
}
    
    else
    {
        printf("Book not found.\n");
    }

    break;
}

            case 5:
    sortBooksByID(&bookHead);
    break;

            case 6:
            {
                Member newMember = {0};

                printf("Enter Member ID: ");
                scanf("%d", &newMember.memberID);

                printf("Enter Registration Number: ");
                scanf(" %29s", newMember.registrationNo);

                printf("Enter Name: ");
                scanf(" %99[^\n]", newMember.name);

                printf("Enter Email: ");
                scanf(" %99s", newMember.email);

                printf("Enter Phone: ");
                scanf(" %19s", newMember.phone);

                printf("Enter Member Type (Student/Faculty): ");
                scanf(" %19s", newMember.memberType);

                newMember.active = 1;

                addMember(&memberHead, &newMember);
                break;
            }

            case 7:
                displayMembers(memberHead);
                break;

            
case 8:
{
    printf("Enter Member ID to search: ");
    scanf("%d", &id);

    Member *found = findMemberByID(memberHead, id);

    if (found != NULL)
    {
        printf("\nMember found!\n");
        printf("Member ID: %d\n", found->memberID);
        printf("Name: %s\n", found->name);
        printf("Registration No: %s\n", found->registrationNo);
        printf("Email: %s\n", found->email);
        printf("Phone: %s\n", found->phone);
        printf("Member Type: %s\n", found->memberType);
        printf("Status: %s\n",
               found->active ? "Active" : "Inactive");

        free(found);
    }
    else
    {
        printf("Member not found.\n");
    }

    break;
}

            case 9:
                printf("Enter Book ID to deactivate: ");
                scanf("%d", &id);
                deactivateBook(bookHead, id);
                break;

            case 10:
                printf("Enter Member ID to deactivate: ");
                scanf("%d", &id);
                deactivateMember(memberHead, id);
                break;

            case 11:
            {
                int memberID, bookID;

                if (readInt("Enter Member ID: ", &memberID) &&
                    readInt("Enter Book ID: ", &bookID))
                {
                    issueBook(memberID, bookID);
                }
                break;
            }

            case 12:
            {
                int loanID;

                if (readInt("Enter Loan ID to return: ", &loanID))
                    returnBook(loanID);
                break;
            }

            case 13:
            {
                int loanID;

                if (readInt("Enter Loan ID to renew: ", &loanID))
                    renewLoan(loanID);
                break;
            }

            case 14:
            {
                int memberID;

                if (readInt("Enter Member ID (0 for all members): ",
                            &memberID))
                {
                    displayActiveLoans(memberID);
                }
                break;
            }

            case 15:
                displayOverdueLoans();
                break;

            case 0:
                printf("Exiting Library Management System...\n");
                break;

            default:
                printf("Invalid choice. Try again.\n");
        }

    } while (choice != 0);

    /* Free allocated book memory */
    while (bookHead != NULL)
    {
        Book *temp = bookHead;
        bookHead = bookHead->next;
        free(temp);
    }

    /* Free allocated member memory */
    while (memberHead != NULL)
    {
        Member *temp = memberHead;
        memberHead = memberHead->next;
        free(temp);
    }
    closeDB();
    return 0;
}