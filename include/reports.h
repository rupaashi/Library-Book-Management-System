#ifndef REPORTS_H
#define REPORTS_H

/* SQL-backed reports. Every report reads live data from MySQL (JOIN,
   GROUP BY, aggregates, ORDER BY and date filters) and prints it as a
   table. Nothing here changes any data.

   Like the other modules: 1 = success, 0 = failure, messages printed.

   Two reports already exist from earlier stages and are NOT repeated:
     - currently overdue loans      -> displayOverdueLoans()   (menu 15)
     - one reservation queue        -> displayReservationQueue (menu 18)
   Menu 20 lists each unpaid fine; reportFinesByMember() below adds the
   per-member summary. */

/* All loans of one student, newest first, with state and fine.
   days > 0 keeps only loans issued in the last `days` days; 0 = all. */
int reportBorrowingHistory(int memberID, int days);

/* Most borrowed titles. days > 0 limits to loans issued in the last
   `days` days (0 = all time). topN is clamped to 1..50 (<= 0 gives 10). */
int reportMostBorrowed(int days, int topN);

/* Unpaid fines grouped per member, biggest debt first, plus totals of
   money still owed and money already collected. */
int reportFinesByMember(void);

/* Loans issued per month: how many are still out and how many were
   returned late. months > 0 = only the last `months` months; 0 = all. */
int reportBorrowingActivity(int months);

/* Reservation queue status: counts per status, then one line per title
   that has live (WAITING or READY) reservations. */
int reportReservationStatus(void);

#endif
