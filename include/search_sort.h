#ifndef SEARCH_SORT_H 
#define SEARCH_SORT_H 
#include "common.h" 
Book *linearSearchByTitle(Book *head, const char *title); 
void sortBooksByID(Book **head); 
void displayBooksByTitle(Book *head);
 #endif