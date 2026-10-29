#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/circulation.h"
#include "../include/config.h"
#include "../include/db.h"
#include "../include/dbhelpers.h"
#include "../include/fine.h"
#include "../include/reservation.h"


/* =================================================================
   ISSUE
   ================================================================= */
int issueBook(int memberID, int bookID)
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

    /* 1. Is the member allowed to borrow? */
    int eligible = checkMemberEligible(conn, memberID, 1);

    if (eligible != 1)
        return abortTx(conn, NULL);

    /* 2. Does the book exist and is it still in the catalogue? */
    int bookParams[1] = { bookID };
    int found = queryRow(conn,
        "SELECT active FROM BOOKS WHERE bookID = ?",
        bookParams, 1, row, 1);

    if (found < 0)
        return abortTx(conn, NULL);

    if (found == 0)
    {
        printf("Book %d does not exist.\n", bookID);
        return abortTx(conn, NULL);
    }

    if (atoi(row[0]) == 0)
    {
        printf("Book %d is inactive and cannot be issued.\n", bookID);
        return abortTx(conn, NULL);
    }

    /* 3. Does this student already hold this title? */
    int pair[2] = { bookID, memberID };
    found = queryRow(conn,
        "SELECT COUNT(*) FROM LOANS "
        "WHERE bookID = ? AND memberID = ? AND returnDate IS NULL",
        pair, 2, row, 1);

    if (found < 0)
        return abortTx(conn, NULL);

    if (atoi(row[0]) > 0)
    {
        printf("Member %d already has this book on loan.\n", memberID);
        return abortTx(conn, NULL);
    }

    /* 4. Take one copy out of stock. The check and the subtraction are
       ONE statement, so two people cannot both take the last copy.
       Copies on hold (READY) for OTHER students do not count as free. */
    int stockParams[2] = { bookID, memberID };
    long long changed = runUpdate(conn,
        "UPDATE BOOKS b SET b.availableCopies = b.availableCopies - 1 "
        "WHERE b.bookID = ? AND b.active = 1 "
        "AND b.availableCopies > ("
        "  SELECT COUNT(*) FROM RESERVATIONS r "
        "  WHERE r.bookID = b.bookID AND r.status = 'READY' "
        "  AND r.memberID <> ?)",
        stockParams, 2, NULL);

    if (changed < 0)
        return abortTx(conn, NULL);

    if (changed == 0)
        return abortTx(conn,
            "No copy is available right now. You can reserve this book.");

    /* 5. Record the loan. */
    int loanParams[3] = { bookID, memberID, LOAN_PERIOD_DAYS };
    unsigned long long loanID = 0;

    changed = runUpdate(conn,
        "INSERT INTO LOANS (bookID, memberID, issueDate, dueDate) "
        "VALUES (?, ?, CURDATE(), DATE_ADD(CURDATE(), INTERVAL ? DAY))",
        loanParams, 3, &loanID);

    if (changed != 1)
        return abortTx(conn, NULL);

    /* 6. Borrowing the title satisfies this student's reservation for it
       (READY = the held copy was used; WAITING = a copy turned up before
       their turn, e.g. they paid a fine that had been blocking them). */
    int resParams[2] = { bookID, memberID };

    if (runUpdate(conn,
            "UPDATE RESERVATIONS SET status = 'FULFILLED' "
            "WHERE bookID = ? AND memberID = ? "
            "AND status IN ('READY', 'WAITING')",
            resParams, 2, NULL) < 0)
        return abortTx(conn, NULL);

    /* 7. Read back the due date, then save everything. */
    int idParam[1] = { (int)loanID };

    if (queryRow(conn, "SELECT dueDate FROM LOANS WHERE loanID = ?",
                 idParam, 1, row, 1) != 1)
        return abortTx(conn, NULL);

    if (!commitTx(conn))
        return 0;

    printf("Book issued successfully! Loan ID: %llu, due date: %s\n",
           loanID, row[0]);
    return 1;
}


/* =================================================================
   RETURN
   ================================================================= */
int returnBook(int loanID)
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
    int idParam[1] = { loanID };

    /* 1. Lock the loan row (FOR UPDATE) so nobody else can return it
       at the same moment, and read what we need. */
    int found = queryRow(conn,
        "SELECT bookID, DATEDIFF(CURDATE(), dueDate), returnDate IS NOT NULL "
        "FROM LOANS WHERE loanID = ? FOR UPDATE",
        idParam, 1, row, 3);

    if (found < 0)
        return abortTx(conn, NULL);

    if (found == 0)
        return abortTx(conn, "Loan not found.");

    if (atoi(row[2]) == 1)
        return abortTx(conn, "This loan was already returned.");

    int bookID   = atoi(row[0]);
    int daysLate = atoi(row[1]);

    /* 2. Close the loan. Only an ACTIVE loan matches, so a duplicate
       return changes 0 rows. */
    long long changed = runUpdate(conn,
        "UPDATE LOANS SET returnDate = CURDATE() "
        "WHERE loanID = ? AND returnDate IS NULL",
        idParam, 1, NULL);

    if (changed < 0)
        return abortTx(conn, NULL);

    if (changed == 0)
        return abortTx(conn, "This loan was already returned.");

    /* 3. Put the copy back on the shelf. The guard keeps the count from
       going above the number of copies the library owns. */
    int bookParam[1] = { bookID };

    changed = runUpdate(conn,
        "UPDATE BOOKS SET availableCopies = availableCopies + 1 "
        "WHERE bookID = ? AND availableCopies < totalCopies",
        bookParam, 1, NULL);

    if (changed < 0)
        return abortTx(conn, NULL);

    if (changed == 0)
        printf("Warning: stock for book %d was already at its maximum. "
               "Ask the catalogue owner to check the copy counts.\n", bookID);

    /* 3b. A copy is back on the shelf: hand it to the oldest eligible
       WAITING reservation (status -> READY). This runs inside the same
       transaction, so a failure here also undoes the return. */
    int promotedIDs[PROMOTE_MAX];
    int promoted = promoteNext(conn, bookID, promotedIDs, PROMOTE_MAX);

    if (promoted < 0)
        return abortTx(conn, NULL);

    /* 4. Overdue? Record ONE fine (see fine.c). It runs in this same
       transaction, so a failure here also undoes the return. */
    char fineAmount[CELL] = "";

    if (createFine(conn, loanID, daysLate, fineAmount, sizeof(fineAmount)) < 0)
        return abortTx(conn, NULL);

    if (!commitTx(conn))
        return 0;

    printf("Book returned successfully!\n");

    if (daysLate > 0)
        printf("Returned %d day(s) late. Fine recorded: Rs %s (unpaid).\n",
               daysLate, fineAmount);

    /* Tell the librarian who the copy is being held for. */
    for (int i = 0; i < promoted && i < PROMOTE_MAX; i++)
        announceReady(promotedIDs[i]);

    return 1;
}


/* =================================================================
   RENEW
   ================================================================= */
int renewLoan(int loanID)
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
    int idParam[1] = { loanID };

    int found = queryRow(conn,
        "SELECT renewalCount, returnDate IS NOT NULL, dueDate < CURDATE(), "
        "bookID, memberID FROM LOANS WHERE loanID = ? FOR UPDATE",
        idParam, 1, row, 5);

    if (found < 0)
        return abortTx(conn, NULL);

    if (found == 0)
        return abortTx(conn, "Loan not found.");

    if (atoi(row[1]) == 1)
        return abortTx(conn, "This loan was already returned.");

    if (atoi(row[2]) == 1)
        return abortTx(conn,
            "This loan is overdue and cannot be renewed. Please return it.");

    if (atoi(row[0]) >= MAX_RENEWALS)
    {
        printf("This loan has reached the limit of %d renewals.\n",
               MAX_RENEWALS);
        return abortTx(conn, NULL);
    }

    int bookID   = atoi(row[3]);
    int memberID = atoi(row[4]);

    int eligible = checkMemberEligible(conn, memberID, 1);

    if (eligible != 1)
        return abortTx(conn, NULL);

    /* Someone else waiting for this title? Then it must go back. */
    int waitParams[2] = { bookID, memberID };

    found = queryRow(conn,
        "SELECT COUNT(*) FROM RESERVATIONS "
        "WHERE bookID = ? AND memberID <> ? "
        "AND status IN ('WAITING', 'READY')",
        waitParams, 2, row, 1);

    if (found < 0)
        return abortTx(conn, NULL);

    if (atoi(row[0]) > 0)
        return abortTx(conn,
            "Another student has reserved this book, so it cannot be renewed.");

    /* Restart the loan period from today. */
    int renewParams[3] = { LOAN_PERIOD_DAYS, loanID, MAX_RENEWALS };

    long long changed = runUpdate(conn,
        "UPDATE LOANS SET dueDate = DATE_ADD(CURDATE(), INTERVAL ? DAY), "
        "renewalCount = renewalCount + 1 "
        "WHERE loanID = ? AND returnDate IS NULL AND renewalCount < ?",
        renewParams, 3, NULL);

    if (changed < 0)
        return abortTx(conn, NULL);

    if (changed == 0)
        return abortTx(conn, "Renewal not possible.");

    if (queryRow(conn,
            "SELECT dueDate, renewalCount FROM LOANS WHERE loanID = ?",
            idParam, 1, row, 2) != 1)
        return abortTx(conn, NULL);

    if (!commitTx(conn))
        return 0;

    printf("Loan renewed! New due date: %s (renewal %s of %d)\n",
           row[0], row[1], MAX_RENEWALS);
    return 1;
}


/* =================================================================
   VIEWS
   ================================================================= */

/* Print every row returned by a loan query. The query must return
   exactly these 7 columns, in this order:
   loanID, member name, title, issueDate, dueDate, renewalCount,
   days overdue. */
static void printLoanRows(MYSQL *conn, const char *sql,
                          int *params, int nParams,
                          const char *emptyMessage, int showFine)
{
    MYSQL_STMT *stmt = runStmt(conn, sql, params, nParams);

    if (stmt == NULL)
        return;

    char row[MAX_COLS][CELL];
    unsigned long lengths[MAX_COLS];
    NullFlag nulls[MAX_COLS];
    MYSQL_BIND res[MAX_COLS];

    bindResults(res, row, lengths, nulls, 7);

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
        finishCells(row, lengths, nulls, 7);
        count++;

        int daysOverdue = atoi(row[6]);

        printf("\nLoan ID: %s\n", row[0]);
        printf("Member: %s\n", row[1]);
        printf("Book: %s\n", row[2]);
        printf("Issued: %s   Due: %s\n", row[3], row[4]);
        printf("Renewals: %s\n", row[5]);

        if (daysOverdue > 0)
        {
            printf("Days overdue: %d\n", daysOverdue);

            if (showFine)
                printf("Fine if returned today: Rs %ld\n",
                       calculateFineAmount(daysOverdue));
        }

        printf("-------------------------\n");
    }

    if (rc == 1)
        printf("Database error (fetch): %s\n", mysql_stmt_error(stmt));

    if (count == 0)
        printf("%s\n", emptyMessage);

    mysql_stmt_close(stmt);
}

void displayActiveLoans(int memberID)
{
    MYSQL *conn = getConnection();

    if (conn == NULL)
    {
        printf("Database is not connected!\n");
        return;
    }

    /* "? <= 0 OR l.memberID = ?" lets one query serve both
       "one member" and "everyone" (memberID <= 0). */
    int params[2] = { memberID, memberID };

    printLoanRows(conn,
        "SELECT l.loanID, m.name, b.title, l.issueDate, l.dueDate, "
        "l.renewalCount, GREATEST(DATEDIFF(CURDATE(), l.dueDate), 0) "
        "FROM LOANS l "
        "JOIN MEMBERS m ON m.memberID = l.memberID "
        "JOIN BOOKS b ON b.bookID = l.bookID "
        "WHERE l.returnDate IS NULL AND (? <= 0 OR l.memberID = ?) "
        "ORDER BY l.dueDate, l.loanID",
        params, 2, "No active loans found.", 0);
}

void displayOverdueLoans(void)
{
    MYSQL *conn = getConnection();

    if (conn == NULL)
    {
        printf("Database is not connected!\n");
        return;
    }

    printLoanRows(conn,
        "SELECT l.loanID, m.name, b.title, l.issueDate, l.dueDate, "
        "l.renewalCount, DATEDIFF(CURDATE(), l.dueDate) "
        "FROM LOANS l "
        "JOIN MEMBERS m ON m.memberID = l.memberID "
        "JOIN BOOKS b ON b.bookID = l.bookID "
        "WHERE l.returnDate IS NULL AND l.dueDate < CURDATE() "
        "ORDER BY l.dueDate, l.loanID",
        NULL, 0, "No overdue loans.", 1);
}