
#ifndef MEMBER_H
#define MEMBER_H

#include "common.h"

int addMember(Member **head, const Member *input);
Member *findMemberByID(Member *head, int memberID);
void displayMembers(Member *head);
int deactivateMember(Member *head, int memberID);

#endif