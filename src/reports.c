#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/reports.h"
#include "../include/db.h"
#include "../include/dbhelpers.h"

/* =================================================================
   Report helper (private to this file).

   Runs ANY SELECT through the shared prepared-statement helpers and
   prints the rows as an aligned text table.

     headers[i] / widths[i] : title and print width of column i
     Values longer than the width are cut, so a long book title cannot
     push the other columns out of line.

   All values arrive as text (see bindResults), so DECIMAL money like
   "30.00" is printed exactly as MySQL stores it.

   Returns the number of rows printed, or -1 on a database error.
   ================================================================= */
static int printTable(MYSQL *conn, const char *sql,
                      int *params, int nParams, int nCols,
                      const char *const *headers, const int *widths)
{
    MYSQL_STMT *stmt = runStmt(conn, sql, params, nParams);

    if (stmt == NULL)
        return -1;

    char row[MAX_COLS][CELL];
    unsigned long lengths[MAX_COLS];
    NullFlag nulls[MAX_COLS];
    MYSQL_BIND res[MAX_COLS];

    bindResults(res, row, lengths, nulls, nCols);

    if (mysql_stmt_bind_result(stmt, res) != 0 ||
        mysql_stmt_store_result(stmt) != 0)
    {
        printf("Database error (result): %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }

    int count = 0;
    int rc;

    while ((rc = mysql_stmt_fetch(stmt)) == 0 || rc == MYSQL_DATA_TRUNCATED)
    {
        if (count == 0)
        {
            /* Header and rule are printed only when there is a row. */
            int total = 0;

            for (int i = 0; i < nCols; i++)
            {
                printf("%-*.*s  ", widths[i], widths[i], headers[i]);
                total += widths[i] + 2;
            }
            printf("\n");

            for (int i = 0; i < total; i++)
                putchar('-');
            printf("\n");
        }

        finishCells(row, lengths, nulls, nCols);

        for (int i = 0; i < nCols; i++)
            printf("%-*.*s  ", widths[i], widths[i], row[i]);
        printf("\n");

        count++;
    }

    if (rc == 1)
    {
        printf("Database error (fetch): %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }

    mysql_stmt_close(stmt);
    return count;
}

/* Every report starts the same way: make sure we are connected. */
static MYSQL *reportConnection(void)
{
    MYSQL *conn = getConnection();

    if (conn == NULL)
        printf("Database is not connected!\n");

    return conn;
}


/* =================================================================
   REPORT 1: borrowing history of one student

   JOINs: LOANS -> BOOKS (title)  and  LOANS -> FINES (optional).
   LEFT JOIN FINES keeps loans that have no fine; an inner JOIN would
   silently drop them. The state column is derived, not stored:
   OVERDUE only means "not returned and past the due date".

   Date filter: "? <= 0 OR issueDate >= DATE_SUB(CURDATE(), ...)" gives
   both "everything" (days = 0) and "last N days" with ONE statement.
   ================================================================= */
int reportBorrowingHistory(int memberID, int days)
{
    MYSQL *conn = reportConnection();

    if (conn == NULL)
        return 0;

    if (days < 0)
        days = 0;

    char row[MAX_COLS][CELL];
    int idParam[1] = { memberID };

    int found = queryRow(conn,
        "SELECT name, registrationNo FROM MEMBERS WHERE memberID = ?",
        idParam, 1, row, 2);

    if (found < 0)
        return 0;

    if (found == 0)
    {
        printf("Member %d does not exist.\n", memberID);
        return 0;
    }

    printf("\nBorrowing history: %s (%s)\n", row[0], row[1]);

    if (days > 0)
        printf("Loans issued in the last %d day(s)\n", days);

    printf("\n");

    int params[3] = { memberID, days, days };

    static const char *const headers[] =
        { "Loan", "Title", "Issued", "Due", "Returned", "State", "Fine" };
    static const int widths[] = { 5, 28, 10, 10, 10, 8, 16 };

    int rows = printTable(conn,
        "SELECT l.loanID, b.title, l.issueDate, l.dueDate, "
        "       COALESCE(DATE_FORMAT(l.returnDate, '%Y-%m-%d'), '-'), "
        "       CASE WHEN l.returnDate IS NOT NULL THEN 'RETURNED' "
        "            WHEN l.dueDate < CURDATE()    THEN 'OVERDUE' "
        "            ELSE 'ACTIVE' END, "
        "       COALESCE(CONCAT('Rs ', f.amount, ' ', f.status), '-') "
        "FROM LOANS l "
        "JOIN BOOKS b ON b.bookID = l.bookID "
        "LEFT JOIN FINES f ON f.loanID = l.loanID "
        "WHERE l.memberID = ? "
        "  AND (? <= 0 OR l.issueDate >= DATE_SUB(CURDATE(), INTERVAL ? DAY)) "
        "ORDER BY l.issueDate DESC, l.loanID DESC",
        params, 3, 7, headers, widths);

    if (rows < 0)
        return 0;

    if (rows == 0)
    {
        printf("No loans found.\n");
        return 1;
    }

    /* Summary line: counted by MySQL (aggregate), not by C. SUM() over a
       condition adds 1 for every row where the condition is true. */
    if (queryRow(conn,
            "SELECT COUNT(*), COALESCE(SUM(returnDate IS NULL), 0) "
            "FROM LOANS "
            "WHERE memberID = ? "
            "  AND (? <= 0 OR issueDate >= DATE_SUB(CURDATE(), INTERVAL ? DAY))",
            params, 3, row, 2) == 1)
        printf("\nTotal loans: %s (%s still out)\n", row[0], row[1]);

    return 1;
}


/* =================================================================
   REPORT 2: most borrowed titles

   GROUP BY collapses all loans of one title into one row, COUNT(*)
   counts them. COUNT(DISTINCT memberID) shows how many different
   students borrowed it, so one student re-borrowing does not look like
   popularity. Ties are broken by title and ID, so the order is stable.
   LIMIT keeps only the top N.
   ================================================================= */
int reportMostBorrowed(int days, int topN)
{
    MYSQL *conn = reportConnection();

    if (conn == NULL)
        return 0;

    if (days < 0)
        days = 0;
    if (topN <= 0)
        topN = 10;
    if (topN > 50)
        topN = 50;

    printf("\nMost borrowed titles (top %d, %s)\n\n", topN,
           days > 0 ? "recent loans only" : "all time");

    int params[3] = { days, days, topN };

    static const char *const headers[] =
        { "ID", "Title", "Author", "Loans", "Students" };
    static const int widths[] = { 5, 30, 20, 5, 8 };

    int rows = printTable(conn,
        "SELECT b.bookID, b.title, b.author, "
        "       COUNT(*), COUNT(DISTINCT l.memberID) "
        "FROM LOANS l "
        "JOIN BOOKS b ON b.bookID = l.bookID "
        "WHERE ? <= 0 OR l.issueDate >= DATE_SUB(CURDATE(), INTERVAL ? DAY) "
        "GROUP BY b.bookID, b.title, b.author "
        "ORDER BY COUNT(*) DESC, b.title, b.bookID "
        "LIMIT ?",
        params, 3, 5, headers, widths);

    if (rows < 0)
        return 0;

    if (rows == 0)
        printf("No loans recorded for this period.\n");

    return 1;
}


/* =================================================================
   REPORT 3: unpaid fines per member

   Menu 20 lists every fine one by one. This report GROUPs them by
   member so the librarian sees who owes the most. SUM() is done by
   MySQL on DECIMAL values, so no rupee is lost to rounding.
   ================================================================= */
int reportFinesByMember(void)
{
    MYSQL *conn = reportConnection();

    if (conn == NULL)
        return 0;

    printf("\nOutstanding fines by member\n\n");

    static const char *const headers[] =
        { "Member", "Name", "Fines", "Owes (Rs)", "Oldest" };
    static const int widths[] = { 6, 28, 5, 10, 10 };

    int rows = printTable(conn,
        "SELECT m.memberID, m.name, COUNT(*), SUM(f.amount), MIN(f.createdAt) "
        "FROM FINES f "
        "JOIN LOANS   l ON l.loanID   = f.loanID "
        "JOIN MEMBERS m ON m.memberID = l.memberID "
        "WHERE f.status = 'UNPAID' "
        "GROUP BY m.memberID, m.name "
        "ORDER BY SUM(f.amount) DESC, m.memberID",
        NULL, 0, 5, headers, widths);

    if (rows < 0)
        return 0;

    if (rows == 0)
        printf("No unpaid fines.\n");

    /* One pass over FINES gives both totals. CASE picks which rows each
       SUM() adds up. */
    char row[MAX_COLS][CELL];

    int ok = queryRow(conn,
        "SELECT COALESCE(SUM(CASE WHEN status = 'UNPAID' THEN amount END), 0), "
        "       COALESCE(SUM(CASE WHEN status = 'PAID'   THEN amount END), 0), "
        "       COALESCE(SUM(status = 'UNPAID'), 0), "
        "       COALESCE(SUM(status = 'PAID'), 0) "
        "FROM FINES",
        NULL, 0, row, 4);

    if (ok == 1)
        printf("\nStill owed: Rs %s (%s fine(s))\n"
               "Collected so far: Rs %s (%s fine(s))\n",
               row[0], row[2], row[1], row[3]);

    return 1;
}


/* =================================================================
   REPORT 4: borrowing activity per month

   GROUP BY month turns thousands of loans into one line per month.
   DATE_FORMAT(issueDate, '%Y-%m') makes the month label, e.g. 2026-10.
   "returned late" compares two columns of the same row.
   ================================================================= */
int reportBorrowingActivity(int months)
{
    MYSQL *conn = reportConnection();

    if (conn == NULL)
        return 0;

    if (months < 0)
        months = 0;

    printf("\nBorrowing activity by month (%s)\n\n",
           months > 0 ? "recent months only" : "all time");

    /* "months" counts the current month as 1, so go back months - 1. */
    int params[2] = { months, months > 0 ? months - 1 : 0 };

    static const char *const headers[] =
        { "Month", "Issued", "Still out", "Returned late" };
    static const int widths[] = { 7, 6, 9, 13 };

    /* First day of the current month = today minus (day-of-month - 1)
       days. Going back `months - 1` more months gives the start of the
       window, so months = 1 means "this month only". */
    int rows = printTable(conn,
        "SELECT DATE_FORMAT(issueDate, '%Y-%m'), "
        "       COUNT(*), "
        "       COALESCE(SUM(returnDate IS NULL), 0), "
        "       COALESCE(SUM(returnDate > dueDate), 0) "
        "FROM LOANS "
        "WHERE ? <= 0 "
        "   OR issueDate >= DATE_SUB( "
        "        DATE_SUB(CURDATE(), INTERVAL DAYOFMONTH(CURDATE()) - 1 DAY), "
        "        INTERVAL ? MONTH) "
        "GROUP BY DATE_FORMAT(issueDate, '%Y-%m') "
        "ORDER BY DATE_FORMAT(issueDate, '%Y-%m') DESC",
        params, 2, 4, headers, widths);

    if (rows < 0)
        return 0;

    if (rows == 0)
        printf("No loans recorded for this period.\n");

    return 1;
}


/* =================================================================
   REPORT 5: reservation queue status

   Part A counts reservations by status (GROUP BY status).
   Part B shows, per title, how many students are WAITING or holding a
   READY copy, and when the oldest waiting reservation was made (the
   student who is first in the FIFO queue). Menu 18 shows the full
   named queue of one title.
   ================================================================= */
int reportReservationStatus(void)
{
    MYSQL *conn = reportConnection();

    if (conn == NULL)
        return 0;

    printf("\nReservations by status\n\n");

    static const char *const hA[] = { "Status", "Count" };
    static const int wA[] = { 10, 5 };

    int rows = printTable(conn,
        "SELECT status, COUNT(*) FROM RESERVATIONS "
        "GROUP BY status "
        "ORDER BY FIELD(status, 'WAITING', 'READY', 'FULFILLED', "
        "                       'CANCELLED', 'EXPIRED')",
        NULL, 0, 2, hA, wA);

    if (rows < 0)
        return 0;

    if (rows == 0)
    {
        printf("No reservations have been made yet.\n");
        return 1;
    }

    printf("\nTitles with live reservations\n\n");

    static const char *const hB[] =
        { "ID", "Title", "Free", "Waiting", "Ready", "Oldest waiting" };
    static const int wB[] = { 5, 28, 4, 7, 5, 19 };

    rows = printTable(conn,
        "SELECT b.bookID, b.title, b.availableCopies, "
        "       SUM(r.status = 'WAITING'), "
        "       SUM(r.status = 'READY'), "
        "       COALESCE(MIN(CASE WHEN r.status = 'WAITING' "
        "                         THEN r.reservationDate END), '-') "
        "FROM RESERVATIONS r "
        "JOIN BOOKS b ON b.bookID = r.bookID "
        "WHERE r.status IN ('WAITING', 'READY') "
        "GROUP BY b.bookID, b.title, b.availableCopies "
        "ORDER BY SUM(r.status = 'WAITING') DESC, b.bookID",
        NULL, 0, 6, hB, wB);

    if (rows < 0)
        return 0;

    if (rows == 0)
        printf("No live reservations.\n");

    return 1;
}
