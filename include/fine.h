#ifndef FINE_H
#define FINE_H

#include "db.h"

/* Fines. Money is kept in whole rupees (integers) in C and as DECIMAL in
   MySQL: no floating point anywhere. Policy values live in config.h. */

/* Pure arithmetic, no database: days late * FINE_RATE_PER_DAY.
   Returns 0 when daysLate <= 0. */
long calculateFineAmount(int daysLate);

/* Used by returnBook INSIDE its transaction. Records at most ONE fine for
   the loan (UNIQUE(loanID) makes a retry harmless) and copies the stored
   amount, e.g. "30.00", into amountOut.
   Returns 1 = a fine exists for the loan, 0 = nothing owed (not late),
   -1 = database error. */
int createFine(MYSQL *conn, int loanID, int daysLate,
               char *amountOut, size_t outSize);

/* memberID <= 0 lists the unpaid fines of every member. */
void displayUnpaidFines(int memberID);

/* Pay a fine IN FULL (partial payments are not supported).
   Returns 1 on success, 0 on failure; messages are printed. */
int payFine(int fineID);

#endif
