#ifndef QUEUE_H
#define QUEUE_H

/* A FIFO queue (linked list with front and rear pointers) of reservation
   entries. It holds NO database connection: the reservation module fills
   it from the RESERVATIONS table, in the order the database returns
   them, and empties it while processing. The queue is a short-lived
   working copy; the database is always the source of truth. */

typedef struct ResNode {
    int reservationID;
    int bookID;
    int memberID;
    struct ResNode *next;
} ResNode;

typedef struct {
    ResNode *front;   /* oldest entry: leaves first  */
    ResNode *rear;    /* newest entry: joins here    */
    int size;
} ResQueue;

void queueInit(ResQueue *q);
int  queueIsEmpty(const ResQueue *q);

/* Add at the rear. Returns 1, or 0 if memory ran out. */
int  enqueue(ResQueue *q, int reservationID, int bookID, int memberID);

/* Remove from the front into *out (its next field is set to NULL).
   Returns 1, or 0 if the queue was empty. */
int  dequeue(ResQueue *q, ResNode *out);

/* Free every remaining entry. */
void queueClear(ResQueue *q);

#endif
