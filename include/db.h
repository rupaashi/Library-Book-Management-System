
#ifndef DB_H
#define DB_H

#include <mariadb/mysql.h>

int connectDB(void);
void closeDB(void);
MYSQL *getConnection(void);

#endif