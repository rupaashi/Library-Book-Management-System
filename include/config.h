#ifndef CONFIG_H
#define CONFIG_H

/* ---------------------------------------------------------------
   Library policy settings. These are PROJECT RULES (illustrative
   values), not official policy. Change them here and recompile.
   Money is kept in whole rupees as integers - no floating point.
   --------------------------------------------------------------- */
#define LOAN_PERIOD_DAYS       14   /* days a book may be kept          */
#define FINE_RATE_PER_DAY       5   /* rupees charged per overdue day   */
#define MAX_RENEWALS            2   /* renewals allowed per loan        */
#define RESERVATION_HOLD_DAYS   3   /* days a READY reservation is held */

#endif