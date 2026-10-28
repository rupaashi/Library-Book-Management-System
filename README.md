# Library Book Management System

A console application written in **C** that stores everything in **MySQL**
through the MariaDB Connector/C (prepared statements, transactions).

## Features
| Area | What it does |
|---|---|
| Books and members | Add, display, search, sort, deactivate (linked lists + SQL). |
| Circulation | Issue, return, renew (max 2), active and overdue loans. Inventory changes atomically with the loan. |
| Reservations | Reserve unavailable titles, cancel, FIFO queue (a linked-list queue in C, rebuilt from the database each time), 3-day hold, expiry sweep. |
| Fines | Rs 5 per overdue day, one fine per loan, full payment only. Unpaid fines block borrowing, renewing and reserving. |
| Reports | Student history, most borrowed titles, fines by member, monthly activity, reservation status. |

Policy values (14-day loan, Rs 5/day, 2 renewals, 3-day hold) are project
rules in `include/config.h`. Change them there and rebuild.

## Requirements (Windows)
- MySQL Server 8.x (any computer that can run `mysql`).
- MSYS2 with `mingw-w64-ucrt-x86_64-gcc` and `mingw-w64-ucrt-x86_64-libmariadbclient`;
  `C:\msys64\ucrt64\bin` must be on PATH.

## Set up the database
```
cd sql
mysql -u root -p
mysql> SOURCE setup_all.sql;
```
This creates `library_db` and the five tables. It is safe to re-run.

## Build
From the repo root in PowerShell:
```
.\build.ps1          # builds library.exe
.\build.ps1 -Tests   # also builds the test programs
```

## Run
The password is read from an environment variable and is never stored in the code.
```
$env:LIB_DB_PASS = 'your-password'
.\library.exe
```
Optional: `LIB_DB_HOST` (default `localhost`), `LIB_DB_USER` (`root`), `LIB_DB_NAME` (`library_db`).

## Menu
1-5 books - 6-8 members - 9-10 deactivate - 11-15 circulation -
16-19 reservations - 20-21 fines - 22-26 reports - 0 exit.

## Tests and sample data
Run the setup script in the MySQL client, try the menu, then run the cleanup script.
| Script | Used for |
|---|---|
| `tests/reservation_test_setup.sql` / `_cleanup.sql` | Reservation and fine walkthrough (book 9101, students 9101-9103). |
| `tests/reports_test_setup.sql` / `_cleanup.sql` | Report walkthrough (IDs 9201-9203). |
| `tests/test_circulation.sql` | SQL-only circulation tests (rolled back). |
| `tests/test_queue.c`, `tests/test_fine_calc.c`, `tests/db_test.c` | Unit and connection tests (`.\build.ps1 -Tests`). |

Always compile from the repo root; do not use the editor's Run button, which
compiles only the open file.

## Project layout
```
include/   headers          src/     C modules
sql/       schema scripts   tests/   test programs and sample data
backend/, frontend/   separate web dashboard (not part of the C program)
```
