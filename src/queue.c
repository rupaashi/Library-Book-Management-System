#include <stdlib.h>

#include "../include/queue.h"

void queueInit(ResQueue *q)
{
    q->front = NULL;
    q->rear  = NULL;
    q->size  = 0;
}

int queueIsEmpty(const ResQueue *q)
{
    return q->front == NULL;
}

int enqueue(ResQueue *q, int reservationID, int bookID, int memberID)
{
    ResNode *node = malloc(sizeof(ResNode));

    if (node == NULL)
        return 0;

    node->reservationID = reservationID;
    node->bookID        = bookID;
    node->memberID      = memberID;
    node->next          = NULL;

    if (q->rear == NULL)          /* queue was empty */
        q->front = node;
    else
        q->rear->next = node;

    q->rear = node;
    q->size++;
    return 1;
}

int dequeue(ResQueue *q, ResNode *out)
{
    if (q->front == NULL)
        return 0;

    ResNode *node = q->front;

    *out = *node;
    out->next = NULL;

    q->front = node->next;

    if (q->front == NULL)         /* queue became empty */
        q->rear = NULL;

    q->size--;
    free(node);
    return 1;
}

void queueClear(ResQueue *q)
{
    ResNode dummy;

    while (dequeue(q, &dummy))
    {
    }
}
