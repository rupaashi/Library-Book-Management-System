

#include "../include/db.h"
#include <stdio.h>

static MYSQL *conn = NULL;

int connectDB(void)
{
    conn = mysql_init(NULL);

    if (conn == NULL)
    {
        printf("MySQL initialization failed!\n");
        return 0;
    }

    if (mysql_real_connect(
            conn,
            "localhost",
            "root",
            "Pikachu06@",
            "library_db",
            3306,
            NULL,
            0) == NULL)
    {
        printf("Database connection failed: %s\n",
               mysql_error(conn));

        mysql_close(conn);
        conn = NULL;
        return 0;
    }

    printf("Database connected successfully!\n");
    return 1;
}

MYSQL *getConnection(void)
{
    return conn;
}

void closeDB(void)
{
    if (conn != NULL)
    {
        mysql_close(conn);
        conn = NULL;
        printf("Database connection closed.\n");
    }
}