
#ifndef COMMON_H
#define COMMON_H

typedef struct Book {
    int bookID;
    char title[100];
    char author[100];
    char isbn[20];
    char publisher[100];
    char category[50];
    int publicationYear;

    int totalCopies;
    int availableCopies;
    int damagedCopies;
    int lostCopies;
    int active;

    struct Book *next;
} Book;

typedef struct Member {
    int memberID;
    char registrationNo[30];
    char name[100];
    char email[100];
    char phone[20];
    char memberType[20];
    int active;

    struct Member *next;
} Member;

#endif