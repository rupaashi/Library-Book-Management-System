
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/member.h"
#include "../include/db.h"

int addMember(Member **head, const Member *input)
{
    MYSQL *conn = getConnection();

    if (conn == NULL)
    {
        printf("Database is not connected!\n");
        return 0;
    }
Member *newMember = malloc(sizeof(Member));

if (newMember == NULL)
{
    printf("Memory allocation failed!\n");
    return 0;
}

*newMember = *input;
newMember->next = NULL;

    char regNo[61], name[201], email[201];
    char phone[41], memberType[41];
    char query[1200];

    mysql_real_escape_string(conn, regNo, input->registrationNo,
                             (unsigned long)strlen(input->registrationNo));
    mysql_real_escape_string(conn, name, input->name,
                             (unsigned long)strlen(input->name));
    mysql_real_escape_string(conn, email, input->email,
                             (unsigned long)strlen(input->email));
    mysql_real_escape_string(conn, phone, input->phone,
                             (unsigned long)strlen(input->phone));
    mysql_real_escape_string(conn, memberType, input->memberType,
                             (unsigned long)strlen(input->memberType));

    snprintf(query, sizeof(query),
        "INSERT INTO MEMBERS "
        "(memberID, registrationNo, name, email, phone, memberType, active) "
        "VALUES (%d, '%s', '%s', '%s', '%s', '%s', %d)",
        input->memberID, regNo, name, email, phone,
        memberType, input->active);

    if (mysql_query(conn, query) != 0)
    {
        printf("Could not add member: %s\n", mysql_error(conn));
        free(newMember);
        return 0;
    }

    *newMember = *input;
    newMember->next = NULL;

    if (*head == NULL)
    {
        *head = newMember;
    }
    else
    {
        Member *temp = *head;

        while (temp->next != NULL)
            temp = temp->next;

        temp->next = newMember;
    }

    printf("Member added to database successfully!\n");
    return 1;
}


Member *findMemberByID(Member *head, int memberID)
{
    (void)head;

    MYSQL *conn = getConnection();

    if (conn == NULL)
    {
        printf("Database is not connected!\n");
        return NULL;
    }

    char query[256];

    snprintf(query, sizeof(query),
             "SELECT memberID, registrationNo, name, email, "
             "phone, memberType, active "
             "FROM MEMBERS WHERE memberID = %d",
             memberID);

    if (mysql_query(conn, query) != 0)
    {
        printf("Member search failed: %s\n", mysql_error(conn));
        return NULL;
    }

    MYSQL_RES *result = mysql_store_result(conn);

    if (result == NULL)
    {
        printf("Could not retrieve member: %s\n",
               mysql_error(conn));
        return NULL;
    }

    MYSQL_ROW row = mysql_fetch_row(result);

    if (row == NULL)
    {
        mysql_free_result(result);
        return NULL;
    }

    Member *found = malloc(sizeof(Member));

    if (found == NULL)
    {
        printf("Memory allocation failed!\n");
        mysql_free_result(result);
        return NULL;
    }

    found->memberID = atoi(row[0]);

    snprintf(found->registrationNo,
             sizeof(found->registrationNo), "%s",
             row[1] ? row[1] : "");

    snprintf(found->name, sizeof(found->name), "%s",
             row[2] ? row[2] : "");

    snprintf(found->email, sizeof(found->email), "%s",
             row[3] ? row[3] : "");

    snprintf(found->phone, sizeof(found->phone), "%s",
             row[4] ? row[4] : "");

    snprintf(found->memberType, sizeof(found->memberType), "%s",
             row[5] ? row[5] : "");

    found->active = atoi(row[6]);
    found->next = NULL;

    mysql_free_result(result);

    return found;
}

void displayMembers(Member *head)
{
    (void)head;

    MYSQL *conn = getConnection();

    if (conn == NULL)
    {
        printf("Database is not connected!\n");
        return;
    }

    if (mysql_query(conn,
        "SELECT memberID, name, registrationNo, email "
        "FROM MEMBERS ORDER BY memberID") != 0)
    {
        printf("Could not retrieve members: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *result = mysql_store_result(conn);

    if (result == NULL)
    {
        printf("Could not read members: %s\n", mysql_error(conn));
        return;
    }

    if (mysql_num_rows(result) == 0)
    {
        printf("No members available.\n");
        mysql_free_result(result);
        return;
    }

    MYSQL_ROW row;

    while ((row = mysql_fetch_row(result)) != NULL)
    {
        printf("\nMember ID: %s\n", row[0]);
        printf("Name: %s\n", row[1] ? row[1] : "");
        printf("Registration No: %s\n", row[2] ? row[2] : "");
        printf("Email: %s\n", row[3] ? row[3] : "");
        printf("-------------------------\n");
    }

    mysql_free_result(result);
}

int deactivateMember(Member *head, int memberID)
{
    (void)head;

    MYSQL *conn = getConnection();

    if (conn == NULL)
    {
        printf("Database is not connected!\n");
        return 0;
    }

    char query[150];

    snprintf(query, sizeof(query),
             "UPDATE MEMBERS SET active = 0 WHERE memberID = %d",
             memberID);

    if (mysql_query(conn, query) != 0)
    {
        printf("Could not deactivate member: %s\n",
               mysql_error(conn));
        return 0;
    }

    if (mysql_affected_rows(conn) == 0)
    {
        printf("Member not found or already inactive.\n");
        return 0;
    }

    printf("Member deactivated successfully!\n");
    return 1;
}