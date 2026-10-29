/* Connection smoke test: connects, prints the database and server
   version, and counts the tables. Run it before anything else. */
#include <stdio.h>
#include "../include/db.h"

int main(void)
{
    if (!connectDB())
        return 1;

    MYSQL *conn = getConnection();

    if (mysql_query(conn, "SELECT DATABASE(), VERSION()") != 0)
    {
        printf("Query failed: %s\n", mysql_error(conn));
        closeDB();
        return 1;
    }

    MYSQL_RES *result = mysql_store_result(conn);
    MYSQL_ROW row = (result != NULL) ? mysql_fetch_row(result) : NULL;

    if (row != NULL)
        printf("Database: %s | Server version: %s\n", row[0], row[1]);

    if (result != NULL)
        mysql_free_result(result);

    if (mysql_query(conn, "SHOW TABLES") == 0)
    {
        result = mysql_store_result(conn);

        if (result != NULL)
        {
            printf("Tables found: %llu\n",
                   (unsigned long long)mysql_num_rows(result));
            mysql_free_result(result);
        }
    }

    closeDB();
    return 0;
}