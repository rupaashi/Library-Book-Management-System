#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/dbhelpers.h"

/* =================================================================
   Prepared-statement helpers.

   A prepared statement sends the SQL text to the server first, with
   "?" placeholders, and sends the values separately. The values are
   never pasted into the SQL text, so user input cannot change what
   the SQL means (this is what prevents SQL injection).
   ================================================================= */

/* Prepare, bind integer parameters and execute.
   Returns the executed statement (caller must mysql_stmt_close it),
   or NULL after printing the database error. */
MYSQL_STMT *runStmt(MYSQL *conn, const char *sql,
                    int *params, int nParams)
{
    MYSQL_STMT *stmt = mysql_stmt_init(conn);

    if (stmt == NULL)
    {
        printf("Memory allocation failed!\n");
        return NULL;
    }

    if (mysql_stmt_prepare(stmt, sql, (unsigned long)strlen(sql)) != 0)
    {
        printf("Database error (prepare): %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return NULL;
    }

    if (nParams > 0)
    {
        MYSQL_BIND binds[MAX_PARAMS];
        memset(binds, 0, sizeof(binds));

        for (int i = 0; i < nParams; i++)
        {
            binds[i].buffer_type = MYSQL_TYPE_LONG;   /* 4-byte int */
            binds[i].buffer = &params[i];
        }

        if (mysql_stmt_bind_param(stmt, binds) != 0)
        {
            printf("Database error (bind): %s\n", mysql_stmt_error(stmt));
            mysql_stmt_close(stmt);
            return NULL;
        }
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        printf("Database error: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return NULL;
    }

    return stmt;
}

/* Run an INSERT/UPDATE. Returns the number of rows changed, or -1 on
   error. If insertID is not NULL it receives the new AUTO_INCREMENT id. */
long long runUpdate(MYSQL *conn, const char *sql,
                    int *params, int nParams,
                    unsigned long long *insertID)
{
    MYSQL_STMT *stmt = runStmt(conn, sql, params, nParams);

    if (stmt == NULL)
        return -1;

    long long changed = (long long)mysql_stmt_affected_rows(stmt);

    if (insertID != NULL)
        *insertID = (unsigned long long)mysql_stmt_insert_id(stmt);

    mysql_stmt_close(stmt);
    return changed;
}

/* Describe nCols result columns, all fetched as text. Numbers, dates
   and DECIMAL money values all arrive as strings, so no floating
   point is ever involved. */
void bindResults(MYSQL_BIND *res, char out[][CELL],
                 unsigned long *lengths, NullFlag *nulls, int nCols)
{
    memset(res, 0, sizeof(MYSQL_BIND) * (size_t)nCols);

    for (int i = 0; i < nCols; i++)
    {
        res[i].buffer_type   = MYSQL_TYPE_STRING;
        res[i].buffer        = out[i];
        res[i].buffer_length = CELL - 1;
        res[i].length        = &lengths[i];
        res[i].is_null       = &nulls[i];
    }
}

/* Add the closing '\0' to each fetched value (SQL NULL becomes ""). */
void finishCells(char out[][CELL], unsigned long *lengths,
                 NullFlag *nulls, int nCols)
{
    for (int i = 0; i < nCols; i++)
    {
        if (nulls[i])
            out[i][0] = '\0';
        else
        {
            size_t n = lengths[i] < CELL - 1 ? lengths[i] : CELL - 1;
            out[i][n] = '\0';
        }
    }
}

/* Run a SELECT and read only its FIRST row.
   Returns 1 = row read, 0 = no rows, -1 = error. */
int queryRow(MYSQL *conn, const char *sql, int *params, int nParams,
             char out[][CELL], int nCols)
{
    MYSQL_STMT *stmt = runStmt(conn, sql, params, nParams);

    if (stmt == NULL)
        return -1;

    MYSQL_BIND res[MAX_COLS];
    unsigned long lengths[MAX_COLS];
    NullFlag nulls[MAX_COLS];

    bindResults(res, out, lengths, nulls, nCols);

    if (mysql_stmt_bind_result(stmt, res) != 0 ||
        mysql_stmt_store_result(stmt) != 0)
    {
        printf("Database error (result): %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }

    int rc = mysql_stmt_fetch(stmt);
    int found;

    if (rc == MYSQL_NO_DATA)
        found = 0;
    else if (rc == 1)
    {
        printf("Database error (fetch): %s\n", mysql_stmt_error(stmt));
        found = -1;
    }
    else
    {
        finishCells(out, lengths, nulls, nCols);
        found = 1;
    }

    mysql_stmt_close(stmt);
    return found;
}


/* =================================================================
   Transaction helpers. A transaction makes several statements succeed
   or fail TOGETHER: either COMMIT saves all of them, or ROLLBACK
   undoes all of them.
   ================================================================= */

int beginTx(MYSQL *conn)
{
    if (mysql_query(conn, "START TRANSACTION") != 0)
    {
        printf("Could not start transaction: %s\n", mysql_error(conn));
        return 0;
    }
    return 1;
}

void rollbackTx(MYSQL *conn)
{
    mysql_query(conn, "ROLLBACK");
}

int commitTx(MYSQL *conn)
{
    if (mysql_query(conn, "COMMIT") != 0)
    {
        printf("Could not save changes: %s\n", mysql_error(conn));
        rollbackTx(conn);
        return 0;
    }
    return 1;
}

/* Undo everything done so far, optionally print why, and return 0. */
int abortTx(MYSQL *conn, const char *message)
{
    rollbackTx(conn);

    if (message != NULL)
        printf("%s\n", message);

    return 0;
}


/* =================================================================
   Eligibility check shared by issuing, renewing and reservations.
   Returns 1 = eligible, 0 = not eligible, -1 = error.
   verbose = 1 prints the reason; verbose = 0 stays silent (used when
   the reservation queue quietly skips an ineligible student).
   ================================================================= */
int checkMemberEligible(MYSQL *conn, int memberID, int verbose)
{
    char row[MAX_COLS][CELL];
    int params[1] = { memberID };

    int found = queryRow(conn,
        "SELECT active, memberType = 'Student' "
        "FROM MEMBERS WHERE memberID = ?",
        params, 1, row, 2);

    if (found < 0)
        return -1;

    if (found == 0)
    {
        if (verbose)
            printf("Member %d does not exist.\n", memberID);
        return 0;
    }

    if (atoi(row[0]) == 0)
    {
        if (verbose)
            printf("Member %d is inactive.\n", memberID);
        return 0;
    }

    if (atoi(row[1]) == 0)
    {
        if (verbose)
            printf("Member %d is not a student. Only students can borrow.\n",
                   memberID);
        return 0;
    }

    /* Unpaid fines block borrowing, renewing and reserving (project rule). */
    found = queryRow(conn,
        "SELECT COUNT(*), COALESCE(SUM(f.amount), 0) "
        "FROM FINES f JOIN LOANS l ON l.loanID = f.loanID "
        "WHERE l.memberID = ? AND f.status = 'UNPAID'",
        params, 1, row, 2);

    if (found < 0)
        return -1;

    if (found == 1 && atoi(row[0]) > 0)
    {
        if (verbose)
            printf("Member %d has %s unpaid fine(s) totalling Rs %s. "
                   "Pay them first.\n", memberID, row[0], row[1]);
        return 0;
    }

    return 1;
}
