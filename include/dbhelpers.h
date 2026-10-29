#ifndef DBHELPERS_H
#define DBHELPERS_H

#include "db.h"

/* Shared prepared-statement and transaction helpers, used by the
   circulation and reservation modules (moved out of circulation.c so
   both modules use ONE copy instead of duplicating it). */

#define MAX_PARAMS 4    /* most "?" placeholders any statement uses */
#define MAX_COLS   8    /* most result columns any query returns    */
#define CELL       64   /* characters kept for one result value     */

/* The type of MYSQL_BIND.is_null differs between client library
   versions (char, my_bool or bool), so we ask the compiler for it. */
typedef __typeof__(*((MYSQL_BIND *)0)->is_null) NullFlag;

/* Prepared statements (all parameters are plain ints). */
MYSQL_STMT *runStmt(MYSQL *conn, const char *sql, int *params, int nParams);
long long   runUpdate(MYSQL *conn, const char *sql, int *params, int nParams,
                      unsigned long long *insertID);
void        bindResults(MYSQL_BIND *res, char out[][CELL],
                        unsigned long *lengths, NullFlag *nulls, int nCols);
void        finishCells(char out[][CELL], unsigned long *lengths,
                        NullFlag *nulls, int nCols);
/* 1 = row read, 0 = no rows, -1 = error */
int         queryRow(MYSQL *conn, const char *sql, int *params, int nParams,
                     char out[][CELL], int nCols);

/* Transactions. abortTx rolls back, prints message if not NULL, returns 0. */
int  beginTx(MYSQL *conn);
void rollbackTx(MYSQL *conn);
int  commitTx(MYSQL *conn);
int  abortTx(MYSQL *conn, const char *message);

/* 1 = eligible, 0 = not eligible, -1 = error. verbose = print the reason. */
int checkMemberEligible(MYSQL *conn, int memberID, int verbose);

#endif
