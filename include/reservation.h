#ifndef RESERVATION_H
#define RESERVATION_H

#include "db.h"

/* Like the other modules: 1 = success, 0 = failure, messages printed. */

int  placeReservation(int memberID, int bookID);
int  cancelReservation(int reservationID);

/* bookID <= 0 shows the queues of every title */
void displayReservationQueue(int bookID);

/* Expire READY holds whose expiryDate has passed and pass each freed copy
   to the next waiting student. Returns how many holds expired.
   verbose = 1 also prints a message when there was nothing to expire. */
int  processExpiredReservations(int verbose);

/* ---- used by circulation.c (inside ITS transaction) --------------- */

#define PROMOTE_MAX 5   /* ids reported back per call */

/* Give each free, unheld copy of the title to the oldest eligible WAITING
   reservation (status -> READY). Must be called inside an open
   transaction. Returns how many reservations became READY, or -1 on a
   database error. The first PROMOTE_MAX ids go into promotedIDs. */
int  promoteNext(MYSQL *conn, int bookID, int *promotedIDs, int maxIDs);

/* Print "reservation N is READY for <name>" - call AFTER the commit. */
void announceReady(int reservationID);

#endif
