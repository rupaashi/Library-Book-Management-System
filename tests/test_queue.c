/* Unit test for the FIFO queue. Needs NO database.
   Build:  gcc -Wall -Wextra -Iinclude src/queue.c tests/test_queue.c -o test_queue.exe
   Run:    .\test_queue.exe                                               */
#include <stdio.h>
#include "../include/queue.h"

static int failures = 0;

static void check(const char *what, int ok)
{
    printf("%-52s %s\n", what, ok ? "PASS" : "FAIL");
    if (!ok)
        failures++;
}

int main(void)
{
    ResQueue q;
    ResNode n;

    queueInit(&q);
    check("new queue is empty", queueIsEmpty(&q) && q.size == 0);
    check("dequeue from empty queue returns 0", dequeue(&q, &n) == 0);

    enqueue(&q, 11, 5, 101);          /* oldest  */
    enqueue(&q, 12, 5, 102);
    enqueue(&q, 13, 5, 103);          /* newest  */
    check("size is 3 after three enqueues", q.size == 3);

    check("first out is the first in (FIFO)",
          dequeue(&q, &n) == 1 && n.reservationID == 11 && n.memberID == 101);
    check("second out is the second in",
          dequeue(&q, &n) == 1 && n.reservationID == 12);

    enqueue(&q, 14, 5, 104);          /* joins behind 13 */
    check("a late arrival goes to the back",
          dequeue(&q, &n) == 1 && n.reservationID == 13);
    check("then the late arrival comes out",
          dequeue(&q, &n) == 1 && n.reservationID == 14);

    check("queue empty again, rear reset (reusable)",
          queueIsEmpty(&q) && q.rear == NULL && q.size == 0);

    enqueue(&q, 21, 6, 201);          /* reuse after emptying */
    enqueue(&q, 22, 6, 202);
    queueClear(&q);
    check("queueClear empties it", queueIsEmpty(&q) && q.size == 0);

    printf("\n%s\n", failures == 0 ? "All queue tests passed."
                                   : "Some queue tests FAILED.");
    return failures != 0;
}
