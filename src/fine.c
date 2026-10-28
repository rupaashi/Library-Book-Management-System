#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/fine.h"
#include "../include/config.h"
#include "../include/db.h"
#include "../include/dbhelpers.h"

/* =================================================================
   CALCULATE
   The one and only place where the fine formula lives. A student who
   is 6 days late with FINE_RATE_PER_DAY = 5 owes 6 * 5 = Rs 30.
   ================================================================= */
long calculateFineAmount(int daysLate)
{
    if (daysLate <= 0)
        return 0;

    return (long)daysLate * FINE_RATE_PER_DAY;
}


/* =================================================================
   CREATE (called from returnBook, inside its transaction)

   Why this is safe to retry: FINES has UNIQUE(loanID), so a second
   INSERT for the same loan hits the duplicate-key branch, which does
   "fineID = fineID" (changes nothing). One loan can never be charged
   twice, and a PAID fine is never turned back into an UNPAID one.
   ================================================================= */
int createFine(MYSQL *conn, int loanID, int daysLate,
               char *amountOut, size_t outSize)
{
    long amount = calculateFineAmount(daysLate);

    if (amount <= 0)
        return 0;

    int params[2] = { loanID, (int)amount };

    if (runUpdate(conn,
            "INSERT INTO FINES (loanID, amount) VALUES (?, ?) "
            "ON DUPLICATE KEY UPDATE fineID = fineID",
            params, 2, NULL) < 0)
        return -1;

    /* Read back what is actually stored (also covers the retry case). */
    char row[MAX_COLS][CELL];
    int idParam[1] = { loanID };

    if (queryRow(conn, "SELECT amount FROM FINES WHERE loanID = ?",
                 idParam, 1, row, 1) != 1)
        return -1;

    snprintf(amountOut, outSize, "%s", row[0]);
    return 1;
}


/* =================================================================
   VIEW unpaid fines
   ================================================================= */
void displayUnpaidFines(int memberID)
{
    MYSQL *conn = getConnection();

    if (conn == NULL)
    {
        printf("Database is not connected!\n");
        return;
    }

    /* "? <= 0 OR l.memberID = ?" serves both "one member" and "everyone". */
    int params[2] = { memberID, memberID };

    MYSQL_STMT *stmt = runStmt(conn,
        "SELECT f.fineID, l.loanID, m.name, b.title, f.amount, f.createdAt "
        "FROM FINES f "
        "JOIN LOANS   l ON l.loanID   = f.loanID "
        "JOIN MEMBERS m ON m.memberID = l.memberID "
        "JOIN BOOKS   b ON b.bookID   = l.bookID "
        "WHERE f.status = 'UNPAID' AND (? <= 0 OR l.memberID = ?) "
        "ORDER BY f.createdAt, f.fineID",
        params, 2);

    if (stmt == NULL)
        return;

    char row[MAX_COLS][CELL];
    unsigned long lengths[MAX_COLS];
    NullFlag nulls[MAX_COLS];
    MYSQL_BIND res[MAX_COLS];

    bindResults(res, row, lengths, nulls, 6);

    if (mysql_stmt_bind_result(stmt, res) != 0 ||
        mysql_stmt_store_result(stmt) != 0)
    {
        printf("Database error (result): %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return;
    }

    int count = 0;
    int rc;

    while ((rc = mysql_stmt_fetch(stmt)) == 0 || rc == MYSQL_DATA_TRUNCATED)
    {
        finishCells(row, lengths, nulls, 6);
        count++;

        printf("\nFine ID: %s   (Loan ID: %s)\n", row[0], row[1]);
        printf("Member: %s\n", row[2]);
        printf("Book: %s\n", row[3]);
        printf("Amount: Rs %s   Charged on: %s\n", row[4], row[5]);
        printf("-------------------------\n");
    }

    if (rc == 1)
        printf("Database error (fetch): %s\n", mysql_stmt_error(stmt));

    mysql_stmt_close(stmt);

    if (count == 0)
    {
        printf("No unpaid fines.\n");
        return;
    }

    /* The total is added up by MySQL on DECIMAL values, not in C. */
    if (queryRow(conn,
            "SELECT COUNT(*), COALESCE(SUM(f.amount), 0) "
            "FROM FINES f JOIN LOANS l ON l.loanID = f.loanID "
            "WHERE f.status = 'UNPAID' AND (? <= 0 OR l.memberID = ?)",
            params, 2, row, 2) == 1)
        printf("Total outstanding: Rs %s across %s fine(s)\n",
               row[1], row[0]);
}


/* =================================================================
   PAY (full payment only)
   ================================================================= */
int payFine(int fineID)
{
    MYSQL *conn = getConnection();

    if (conn == NULL)
    {
        printf("Database is not connected!\n");
        return 0;
    }

    if (!beginTx(conn))
        return 0;

    char row[MAX_COLS][CELL];
    int idParam[1] = { fineID };

    /* Lock the fine so two payments of the same fine cannot both succeed. */
    int found = queryRow(conn,
        "SELECT status, amount, paidAt FROM FINES "
        "WHERE fineID = ? FOR UPDATE",
        idParam, 1, row, 3);

    if (found < 0)
        return abortTx(conn, NULL);

    if (found == 0)
        return abortTx(conn, "Fine not found.");

    if (strcmp(row[0], "PAID") == 0)
    {
        printf("Fine %d was already paid on %s.\n", fineID, row[2]);
        return abortTx(conn, NULL);
    }

    char amount[CELL];
    snprintf(amount, sizeof(amount), "%s", row[1]);

    /* status and paidAt change in ONE statement; the table's CHECK rule
       (PAID <=> paidAt is set) is satisfied. Only an UNPAID fine matches,
       so a repeat payment changes 0 rows. */
    long long changed = runUpdate(conn,
        "UPDATE FINES SET status = 'PAID', paidAt = NOW() "
        "WHERE fineID = ? AND status = 'UNPAID'",
        idParam, 1, NULL);

    if (changed < 0)
        return abortTx(conn, NULL);

    if (changed == 0)
        return abortTx(conn, "This fine could not be paid (already paid?).");

    if (!commitTx(conn))
        return 0;

    printf("Fine %d paid in full: Rs %s. Thank you!\n", fineID, amount);
    return 1;
}
