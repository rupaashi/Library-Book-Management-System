#ifndef CIRCULATION_H
#define CIRCULATION_H

/* Each function returns 1 on success and 0 on failure, like the
   book and member modules, and prints its own messages. */

int  issueBook(int memberID, int bookID);
int  returnBook(int loanID);
int  renewLoan(int loanID);

/* memberID <= 0 lists the active loans of every member */
void displayActiveLoans(int memberID);
void displayOverdueLoans(void);

#endif