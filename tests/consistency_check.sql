-- =====================================================================
-- consistency_check.sql - database consistency checks (read-only).
-- Run after ANY testing session. Every query below should return ZERO
-- rows. A returned row is a violation: it shows the offending record.
-- Run:  USE library_db;  SOURCE C:/libsql/consistency_check.sql;
-- =====================================================================
USE library_db;

-- 1. Stock never negative and never above the copies owned.
SELECT 'bad stock' AS problem, bookID, totalCopies, availableCopies
FROM BOOKS
WHERE availableCopies < 0 OR availableCopies > totalCopies;

-- 2. Stock matches the loans still out (books with no damaged/lost copies).
--    availableCopies must equal totalCopies minus active loans.
SELECT 'stock <> copies minus active loans' AS problem,
       b.bookID, b.totalCopies, b.availableCopies, COUNT(l.loanID) AS activeLoans
FROM BOOKS b
LEFT JOIN LOANS l ON l.bookID = b.bookID AND l.returnDate IS NULL
WHERE b.damagedCopies = 0 AND b.lostCopies = 0
GROUP BY b.bookID, b.totalCopies, b.availableCopies
HAVING b.availableCopies <> b.totalCopies - COUNT(l.loanID);

-- 3. A title never holds more READY copies than it has on the shelf.
SELECT 'more READY holds than free copies' AS problem,
       b.bookID, b.availableCopies, COUNT(*) AS readyHolds
FROM RESERVATIONS r JOIN BOOKS b ON b.bookID = r.bookID
WHERE r.status = 'READY'
GROUP BY b.bookID, b.availableCopies
HAVING COUNT(*) > b.availableCopies;

-- 4. READY reservations always have an expiry date; others never need one.
SELECT 'READY without expiryDate' AS problem, reservationID
FROM RESERVATIONS WHERE status = 'READY' AND expiryDate IS NULL;

-- 5. At most one fine per loan, and only for loans that came back late.
SELECT 'fine on a loan that is not late/returned' AS problem, f.fineID, f.loanID
FROM FINES f JOIN LOANS l ON l.loanID = f.loanID
WHERE l.returnDate IS NULL OR l.returnDate <= l.dueDate;

-- 6. Fine amount equals days late x rate (Rs 5 per day, include/config.h).
SELECT 'wrong fine amount' AS problem, f.fineID, f.amount,
       DATEDIFF(l.returnDate, l.dueDate) * 5 AS expected
FROM FINES f JOIN LOANS l ON l.loanID = f.loanID
WHERE f.amount <> DATEDIFF(l.returnDate, l.dueDate) * 5;

-- 7. Renewals never exceed the limit (MAX_RENEWALS = 2).
SELECT 'too many renewals' AS problem, loanID, renewalCount
FROM LOANS WHERE renewalCount > 2;

-- 8. Nothing is both active and past a return date.
SELECT 'return before issue' AS problem, loanID
FROM LOANS WHERE returnDate IS NOT NULL AND returnDate < issueDate;
