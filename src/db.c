#include "../include/db.h"
#include <stdio.h>
#include <stdlib.h>

static MYSQL *conn = NULL;

/* Read a setting from an environment variable.
   Returns the fallback when the variable is missing or empty. */
static const char *getSetting(const char *name, const char *fallback)
{
    const char *value = getenv(name);

    if (value != NULL && value[0] != '\0')
        return value;

    return fallback;
}

int connectDB(void)
{
    /* Credentials come from the environment, never from source code,
       so no password is committed to GitHub. */
    const char *host = getSetting("LIB_DB_HOST", "localhost");
    const char *user = getSetting("LIB_DB_USER", "root");
    const char *pass = getSetting("LIB_DB_PASS", NULL);
    const char *name = getSetting("LIB_DB_NAME", "library_db");

    if (pass == NULL)
    {
        printf("Database password not set. "
               "Set the LIB_DB_PASS environment variable first.\n");
        return 0;
    }

    conn = mysql_init(NULL);

    if (conn == NULL)
    {
        printf("MySQL initialization failed!\n");
        return 0;
    }

    if (mysql_real_connect(
            conn,
            host,
            user,
            pass,
            name,
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