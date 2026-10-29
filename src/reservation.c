#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/reservation.h"
#include "../include/queue.h"
#include "../include/config.h"
#include "../include/db.h"
#include "../include/dbhelpers.h"

/* =================================================================
   Design notes (for the viva)

   * The RESERVATIONS table is the ONLY permanent record. The C queue is
     rebuilt from the table every time it is needed, inside the same
     transaction that then changes the rows, and thrown away afterwards.
     So the queue and the table can never silently disagree.

   * FIFO order is written out explicitly in SQL:
         ORDER BY reservationDate, reservationID
     reservationID breaks ties between two reservations made in the
     same second. We never rely on the order rows happen to be stored.

   * Lock order is always  BOOKS row  ->  RESERVATIONS rows. Issue,
     return, reserve, cancel and expiry all follow it, so two of them
     running together wait for each other instead of deadlocking.

   * "FOR UPDATE" reads take row locks AND see the latest committed
     data. A plain SELECT inside a transaction may see an older
     snapshot, so every decision that matters uses FOR UPDATE.
   ================================================================= */


/* Fill q from a query that returns exactly three columns:
   reservationID, bookID, memberID - in the order they must be served.
   Returns 1, or 0 after printing the problem. */
static int loadQueue(MYSQL *conn, const char *sql,
                     int *params, int nParams, ResQueue *q)
{
    MYSQL_STMT *stmt = runStmt(conn, sql, params, nParams);

    if (stmt == NULL)
        return 0;

    char row[MAX_COLS][CELL];
    unsigned long lengths[MAX_COLS];
    NullFlag nulls[MAX_COLS];
    MYSQL_BIND res[MAX_COLS];

    bindResults(res, row, lengths, nulls, 3);

    if (mysql_stmt_bind_result(stmt, res) != 0 ||
        mysql_stmt_store_result(stmt) != 0)
    {
        printf("Database error (result): %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return 0;
    }

    int ok = 1;
    int rc;

    while ((rc = mysql_stmt_fetch(stmt)) == 0 || rc == MYSQL_DATA_TRUNCATED)
    {
        finishCells(row, lengths, nulls, 3);

        if (!enqueue(q, atoi(row[0]), atoi(row[1]), atoi(row[2])))
        {
            printf("Memory allocation failed!\n");
            ok = 0;
            break;
        }
    }

    if (ok && rc == 1)
    {
        printf("Database error (fetch): %s\n", mysql_stmt_error(stmt));
        ok = 0;
    }

    mysql_stmt_close(stmt);
    return ok;
}


/* =================================================================
   PROMOTE: give free copies to the oldest eligible WAITING students.
   Runs inside the caller's transaction.
   ================================================================= */
int promoteNext(MYSQL *conn, int bookID, int *promotedIDs, int maxIDs)
{
    char row[MAX_COLS][CELL];
    int bookParam[1] = { bookID };

    /* 1. Lock the title's row and read its stock. Inactive or unknown
       titles hand out nothing. */
    int found = queryRow(conn,
        "SELECT availableCopies FROM BOOKS "
        "WHERE bookID = ? AND active = 1 FOR UPDATE",
        bookParam, 1, row, 1);

    if (found < 0)
        return -1;

    if (found == 0)
        return 0;

    int available = atoi(row[0]);

    /* 2. Copies already held (READY) for someone are not free. */
    found = queryRow(conn,
        "SELECT COUNT(*) FROM RESERVATIONS "
        "WHERE bookID = ? AND status = 'READY' FOR UPDATE",
        bookParam, 1, row, 1);

    if (found < 0)
        return -1;

    int freeCopies = available - atoi(row[0]);

    if (freeCopies <= 0)
        return 0;

    /* 3. Load the WAITING reservations into the FIFO queue, oldest first. */
    ResQueue q;
    queueInit(&q);

    if (!loadQueue(conn,
            "SELECT reservationID, bookID, memberID FROM RESERVATIONS "
            "WHERE bookID = ? AND status = 'WAITING' "
            "ORDER BY reservationDate, reservationID FOR UPDATE",
            bookParam, 1, &q))
    {
        queueClear(&q);
        return -1;
    }

    /* 4. Serve the queue from the front. A student who cannot borrow
       right now (inactive, unpaid fine...) is skipped but keeps their
       WAITING status, so they do not lose their place for next time. */
    int promoted = 0;
    ResNode next;

    while (freeCopies > 0 && dequeue(&q, &next))
    {
        int eligible = checkMemberEligible(conn, next.memberID, 0);

        if (eligible < 0)
        {
            queueClear(&q);
            return -1;
        }

        if (eligible == 0)
            continue;

        /* Conditional update: only a still-WAITING row changes. */
        int params[2] = { RESERVATION_HOLD_DAYS, next.reservationID };

        long long changed = runUpdate(conn,
            "UPDATE RESERVATIONS SET status = 'READY', "
            "expiryDate = DATE_ADD(CURDATE(), INTERVAL ? DAY) "
            "WHERE reservationID = ? AND status = 'WAITING'",
            params, 2, NULL);

        if (changed < 0)
        {
            queueClear(&q);
            return -1;
        }

        if (changed == 1)
        {
            if (promoted < maxIDs)
                promotedIDs[promoted] = next.reservationID;

            promoted++;
            freeCopies--;
        }
    }

    queueClear(&q);
    return promoted;
}

void announceReady(int reservationID)
{
    MYSQL *conn = getConnection();

    if (conn == NULL)
        return;

    char row[MAX_COLS][CELL];
    int param[1] = { reservationID };

    int found = queryRow(conn,
        "SELECT r.reservationID, m.name, b.title, r.expiryDate "
        "FROM RESERVATIONS r "
        "JOIN MEMBERS m ON m.memberID = r.memberID "
        "JOIN BOOKS b ON b.bookID = r.bookID "
        "WHERE r.reservationID = ?",
        param, 1, row, 4);

    if (found == 1)
        printf("Reservation %s is now READY: %s can borrow \"%s\" "
               "until %s.\n", row[0], row[1], row[2], row[3]);
}


/* =================================================================
   PLACE
   ================================================================= */
int placeReservation(int memberID, int bookID)
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
    int bookParam[1] = { bookID };

    /* 1. Lock the title first. A return of this title running at the
       same moment must finish before we decide "no copy is free". */
    int found = queryRow(conn,
        "SELECT active, availableCopies FROM BOOKS "
        "WHERE bookID = ? FOR UPDATE",
        bookParam, 1, row, 2);

    if (found < 0)
        return abortTx(conn, NULL);

    if (found == 0)
    {
        printf("Book %d does not exist.\n", bookID);
        return abortTx(conn, NULL);
    }

    if (atoi(row[0]) == 0)
    {
        printf("Book %d is inactive and cannot be reserved.\n", bookID);
        return abortTx(conn, NULL);
    }

    int available = atoi(row[1]);

    /* 2. Is the student allowed to take part? */
    if (checkMemberEligible(conn, memberID, 1) != 1)
        return abortTx(conn, NULL);

    /* 3. Already holding this title? Then there is nothing to wait for. */
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

    /* 4. No duplicate live reservation (the unique key is the backstop). */
    found = queryRow(conn,
        "SELECT COUNT(*) FROM RESERVATIONS "
        "WHERE bookID = ? AND memberID = ? "
        "AND status IN ('WAITING', 'READY') FOR UPDATE",
        pair, 2, row, 1);

    if (found < 0)
        return abortTx(conn, NULL);

    if (atoi(row[0]) > 0)
    {
        printf("Member %d already has an active reservation for this "
               "book.\n", memberID);
        return abortTx(conn, NULL);
    }

    /* 5. Only reserve when no copy is free. Copies held (READY) for
       other students are not free. */
    found = queryRow(conn,
        "SELECT COUNT(*) FROM RESERVATIONS "
        "WHERE bookID = ? AND status = 'READY' FOR UPDATE",
        bookParam, 1, row, 1);

    if (found < 0)
        return abortTx(conn, NULL);

    if (available - atoi(row[0]) > 0)
    {
        printf("A copy of book %d is available now. "
               "Borrow it instead of reserving.\n", bookID);
        return abortTx(conn, NULL);
    }

    /* 6. Join the queue. reservationDate defaults to the current time. */
    unsigned long long resID = 0;

    long long changed = runUpdate(conn,
        "INSERT INTO RESERVATIONS (bookID, memberID) VALUES (?, ?)",
        pair, 2, &resID);

    if (changed != 1)
        return abortTx(conn, NULL);

    /* 7. Work out the place in line: WAITING reservations for this title
       that are at least as old as ours, in FIFO order. */
    int idParam[1] = { (int)resID };

    if (queryRow(conn,
            "SELECT COUNT(*) FROM RESERVATIONS r "
            "JOIN RESERVATIONS me ON me.reservationID = ? "
            "WHERE r.bookID = me.bookID AND r.status = 'WAITING' "
            "AND (r.reservationDate < me.reservationDate "
            "     OR (r.reservationDate = me.reservationDate "
            "         AND r.reservationID <= me.reservationID))",
            idParam, 1, row, 1) != 1)
        return abortTx(conn, NULL);

    if (!commitTx(conn))
        return 0;

    printf("Reservation placed! Reservation ID: %llu, "
           "position in queue: %s\n", resID, row[0]);
    return 1;
}


/* =================================================================
   CANCEL
   ================================================================= */
int cancelReservation(int reservationID)
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
    int idParam[1] = { reservationID };

    /* 1. Which title? (plain read, only to know which row to lock first) */
    int found = queryRow(conn,
        "SELECT bookID FROM RESERVATIONS WHERE reservationID = ?",
        idParam, 1, row, 1);

    if (found < 0)
        return abortTx(conn, NULL);

    if (found == 0)
        return abortTx(conn, "Reservation not found.");

    int bookID = atoi(row[0]);
    int bookParam[1] = { bookID };

    /* 2. Lock the title BEFORE the reservation (see lock-order note). */
    if (queryRow(conn,
            "SELECT bookID FROM BOOKS WHERE bookID = ? FOR UPDATE",
            bookParam, 1, row, 1) < 0)
        return abortTx(conn, NULL);

    /* 3. Re-read the reservation with a lock; its status may have changed. */
    found = queryRow(conn,
        "SELECT status FROM RESERVATIONS WHERE reservationID = ? FOR UPDATE",
        idParam, 1, row, 1);

    if (found < 0)
        return abortTx(conn, NULL);

    if (found == 0)
        return abortTx(conn, "Reservation not found.");

    if (strcmp(row[0], "WAITING") != 0 && strcmp(row[0], "READY") != 0)
    {
        printf("Reservation %d is already %s and cannot be cancelled.\n",
               reservationID, row[0]);
        return abortTx(conn, NULL);
    }

    int wasReady = (strcmp(row[0], "READY") == 0);

    /* 4. Cancel. Only a live reservation matches. */
    long long changed = runUpdate(conn,
        "UPDATE RESERVATIONS SET status = 'CANCELLED' "
        "WHERE reservationID = ? AND status IN ('WAITING', 'READY')",
        idParam, 1, NULL);

    if (changed < 0)
        return abortTx(conn, NULL);

    if (changed == 0)
        return abortTx(conn, "Reservation could not be cancelled.");

    /* 5. A cancelled READY reservation frees its held copy: pass it on. */
    int promotedIDs[PROMOTE_MAX];
    int promoted = 0;

    if (wasReady)
    {
        promoted = promoteNext(conn, bookID, promotedIDs, PROMOTE_MAX);

        if (promoted < 0)
            return abortTx(conn, NULL);
    }

    if (!commitTx(conn))
        return 0;

    printf("Reservation %d cancelled.\n", reservationID);

    for (int i = 0; i < promoted && i < PROMOTE_MAX; i++)
        announceReady(promotedIDs[i]);

    return 1;
}


/* =================================================================
   EXPIRE: a READY hold not collected by its expiryDate is released.
   Each expired hold is handled in its own transaction.
   ================================================================= */
int processExpiredReservations(int verbose)
{
    MYSQL *conn = getConnection();

    if (conn == NULL)
    {
        printf("Database is not connected!\n");
        return 0;
    }

    /* Which holds have expired? Read them into a FIFO queue (plain read;
       each one is re-checked under a lock below before it is changed). */
    ResQueue expired;
    queueInit(&expired);

    if (!loadQueue(conn,
            "SELECT reservationID, bookID, memberID FROM RESERVATIONS "
            "WHERE status = 'READY' AND expiryDate < CURDATE() "
            "ORDER BY expiryDate, reservationID",
            NULL, 0, &expired))
    {
        queueClear(&expired);
        return 0;
    }

    int count = 0;
    ResNode item;

    while (dequeue(&expired, &item))
    {
        if (!beginTx(conn))
            break;

        int bookParam[1] = { item.bookID };
        char row[MAX_COLS][CELL];

        if (queryRow(conn,
                "SELECT bookID FROM BOOKS WHERE bookID = ? FOR UPDATE",
                bookParam, 1, row, 1) < 0)
        {
            abortTx(conn, NULL);
            continue;
        }

        int idParam[1] = { item.reservationID };

        long long changed = runUpdate(conn,
            "UPDATE RESERVATIONS SET status = 'EXPIRED' "
            "WHERE reservationID = ? AND status = 'READY' "
            "AND expiryDate < CURDATE()",
            idParam, 1, NULL);

        if (changed < 0)
        {
            abortTx(conn, NULL);
            continue;
        }

        if (changed == 0)           /* someone collected it meanwhile */
        {
            rollbackTx(conn);
            continue;
        }

        int promotedIDs[PROMOTE_MAX];
        int promoted = promoteNext(conn, item.bookID,
                                   promotedIDs, PROMOTE_MAX);

        if (promoted < 0)
        {
            abortTx(conn, NULL);
            continue;
        }

        if (!commitTx(conn))
            continue;

        count++;
        printf("Reservation %d (member %d) expired - the hold was not "
               "collected in time.\n", item.reservationID, item.memberID);

        for (int i = 0; i < promoted && i < PROMOTE_MAX; i++)
            announceReady(promotedIDs[i]);
    }

    queueClear(&expired);

    if (count == 0 && verbose)
        printf("No expired reservations.\n");

    return count;
}


/* =================================================================
   VIEW
   ================================================================= */
void displayReservationQueue(int bookID)
{
    MYSQL *conn = getConnection();

    if (conn == NULL)
    {
        printf("Database is not connected!\n");
        return;
    }

    /* READY holds first (they already own a copy), then WAITING students
       in FIFO order: reservationDate, then reservationID as tie-breaker. */
    int params[2] = { bookID, bookID };

    MYSQL_STMT *stmt = runStmt(conn,
        "SELECT r.reservationID, r.bookID, b.title, m.name, r.status, "
        "r.reservationDate, COALESCE(r.expiryDate, '') "
        "FROM RESERVATIONS r "
        "JOIN MEMBERS m ON m.memberID = r.memberID "
        "JOIN BOOKS b ON b.bookID = r.bookID "
        "WHERE r.status IN ('WAITING', 'READY') "
        "AND (? <= 0 OR r.bookID = ?) "
        "ORDER BY r.bookID, (r.status = 'WAITING'), "
        "r.reservationDate, r.reservationID",
        params, 2);

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
    int position = 0;
    int lastBook = -1;
    int rc;

    while ((rc = mysql_stmt_fetch(stmt)) == 0 || rc == MYSQL_DATA_TRUNCATED)
    {
        finishCells(row, lengths, nulls, 7);
        count++;

        int thisBook = atoi(row[1]);

        if (thisBook != lastBook)       /* new title: restart numbering */
        {
            lastBook = thisBook;
            position = 0;
            printf("\n=== %s (Book ID %s) ===\n", row[2], row[1]);
        }

        printf("Reservation ID: %s   Member: %s\n", row[0], row[3]);

        if (strcmp(row[4], "READY") == 0)
            printf("  READY - copy held until %s\n", row[6]);
        else
            printf("  WAITING - queue position %d (reserved %s)\n",
                   ++position, row[5]);
    }

    if (rc == 1)
        printf("Database error (fetch): %s\n", mysql_stmt_error(stmt));

    if (count == 0)
        printf("No active reservations.\n");

    mysql_stmt_close(stmt);
}
