/* Unit test for the fine formula. Touches no database rows (it only needs
   to be linked against the same files as the program).
   Build (from the repo root):
     gcc -Wall -Wextra -Iinclude src/fine.c src/dbhelpers.c src/db.c tests/test_fine_calc.c -o test_fine_calc.exe -lmariadb
   Run:  .\test_fine_calc.exe
   Expected values follow FINE_RATE_PER_DAY in include/config.h. */
#include <stdio.h>
#include "../include/fine.h"
#include "../include/config.h"

static int failures = 0;

static void check(const char *what, long got, long expected)
{
    int ok = (got == expected);
    printf("%-34s got %-6ld expected %-6ld %s\n", what, got, expected,
           ok ? "PASS" : "FAIL");
    if (!ok)
        failures++;
}

int main(void)
{
    printf("Fine rate: Rs %d per day\n\n", FINE_RATE_PER_DAY);

    check("on time (0 days late)",       calculateFineAmount(0),   0);
    check("returned early (-3 days)",    calculateFineAmount(-3),  0);
    check("1 day late",                  calculateFineAmount(1),   1L * FINE_RATE_PER_DAY);
    check("6 days late",                 calculateFineAmount(6),   6L * FINE_RATE_PER_DAY);
    check("30 days late",                calculateFineAmount(30),  30L * FINE_RATE_PER_DAY);
    check("a year late (365 days)",      calculateFineAmount(365), 365L * FINE_RATE_PER_DAY);

    printf("\n%s\n", failures == 0 ? "All fine tests passed."
                                   : "Some fine tests FAILED.");
    return failures != 0;
}
