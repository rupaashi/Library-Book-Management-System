-- =====================================================================
-- test_circulation.sql  (Stage 4: test the SQL before writing any C)
-- Uses test rows with IDs 9001/9002 and DELETES them at the end.
-- Run it on a database that already has the tables from 05 and 10.
-- Project rules used here: 14-day loan, Rs 5 per overdue day.
-- =====================================================================
USE library_db;

-- ---------- setup: 1 book with 1 copy, 2 students ----------
DELETE FROM FINES WHERE loanID IN (SELECT loanID FROM LOANS WHERE bookID = 9001);
DELETE FROM LOANS WHERE bookID = 9001;
DELETE FROM BOOKS WHERE bookID = 9001;
DELETE FROM MEMBERS WHERE memberID IN (9001, 9002);

INSERT INTO BOOKS (bookID, title, author, totalCopies, availableCopies)
VALUES (9001, 'Test Book', 'Test Author', 1, 1);
INSERT INTO MEMBERS (memberID, registrationNo, name, memberType)
VALUES (9001, 'TEST-001', 'Student One', 'Student'),
       (9002, 'TEST-002', 'Student Two', 'Student');

-- ---------- ISSUE: student 9001 borrows book 9001 ----------
SELECT 'ISSUE #1 (expect rows_changed = 1, copies left = 0)' AS step;
START TRANSACTION;
UPDATE BOOKS
   SET availableCopies = availableCopies - 1
 WHERE bookID = 9001 AND active = 1 AND availableCopies > 0;
SELECT ROW_COUNT() AS rows_changed;          -- 1 = stock was taken, 0 = refuse
INSERT INTO LOANS (bookID, memberID, issueDate, dueDate)
VALUES (9001, 9001, CURDATE(), DATE_ADD(CURDATE(), INTERVAL 14 DAY));
COMMIT;
SELECT availableCopies AS copies_left FROM BOOKS WHERE bookID = 9001;

-- ---------- ISSUE with no stock: student 9002 tries ----------
SELECT 'ISSUE #2 (expect rows_changed = 0, so the app must ROLLBACK)' AS step;
START TRANSACTION;
UPDATE BOOKS
   SET availableCopies = availableCopies - 1
 WHERE bookID = 9001 AND active = 1 AND availableCopies > 0;
SELECT ROW_COUNT() AS rows_changed;
ROLLBACK;
SELECT availableCopies AS copies_left_still FROM BOOKS WHERE bookID = 9001;

-- ---------- make the loan 3 days overdue ----------
UPDATE LOANS
   SET issueDate = DATE_SUB(CURDATE(), INTERVAL 17 DAY),
       dueDate   = DATE_SUB(CURDATE(), INTERVAL 3 DAY)
 WHERE bookID = 9001 AND memberID = 9001 AND returnDate IS NULL;

SELECT 'OVERDUE REPORT (expect 1 row, days_overdue = 3)' AS step;
SELECT l.loanID, m.name, b.title, l.dueDate,
       DATEDIFF(CURDATE(), l.dueDate) AS days_overdue
  FROM LOANS l
  JOIN MEMBERS m ON m.memberID = l.memberID
  JOIN BOOKS   b ON b.bookID   = l.bookID
 WHERE l.returnDate IS NULL AND l.dueDate < CURDATE()
   AND l.bookID = 9001
 ORDER BY l.dueDate;

-- ---------- RETURN (atomic) ----------
SELECT 'RETURN #1 (expect fine 15.00, copies back to 1)' AS step;
SET @loan = (SELECT loanID FROM LOANS
              WHERE bookID = 9001 AND returnDate IS NULL LIMIT 1);
START TRANSACTION;
-- lock the loan row; if it is already returned this finds nothing
SELECT loanID, bookID FROM LOANS
 WHERE loanID = @loan AND returnDate IS NULL FOR UPDATE;
UPDATE LOANS SET returnDate = CURDATE()
 WHERE loanID = @loan AND returnDate IS NULL;
SELECT ROW_COUNT() AS loan_closed;           -- 1 = ok, 0 = duplicate return
UPDATE BOOKS SET availableCopies = availableCopies + 1
 WHERE bookID = (SELECT bookID FROM LOANS WHERE loanID = @loan)
   AND availableCopies < totalCopies;
-- fine = overdue days x 5.00 ; the UNIQUE(loanID) key makes a retry harmless
INSERT INTO FINES (loanID, amount)
SELECT loanID, DATEDIFF(returnDate, dueDate) * 5.00
  FROM LOANS
 WHERE loanID = @loan AND returnDate > dueDate
ON DUPLICATE KEY UPDATE fineID = fineID;
COMMIT;
SELECT availableCopies AS copies_now FROM BOOKS WHERE bookID = 9001;
SELECT loanID, amount, status, paidAt FROM FINES WHERE loanID = @loan;

-- ---------- duplicate return must change nothing ----------
SELECT 'RETURN #2 same loan (expect loan_closed = 0, so ROLLBACK)' AS step;
START TRANSACTION;
UPDATE LOANS SET returnDate = CURDATE()
 WHERE loanID = @loan AND returnDate IS NULL;
SELECT ROW_COUNT() AS loan_closed;
ROLLBACK;
SELECT availableCopies AS copies_still, (SELECT COUNT(*) FROM FINES WHERE loanID = @loan) AS fine_rows
  FROM BOOKS WHERE bookID = 9001;

-- ---------- unpaid fines for a student ----------
SELECT 'UNPAID FINES for student 9001 (expect 15.00)' AS step;
SELECT m.name, f.fineID, f.amount, f.createdAt
  FROM FINES f
  JOIN LOANS   l ON l.loanID   = f.loanID
  JOIN MEMBERS m ON m.memberID = l.memberID
 WHERE l.memberID = 9001 AND f.status = 'UNPAID';

-- ---------- pay in full ----------
SELECT 'PAY FINE #1 (expect rows_changed = 1)' AS step;
UPDATE FINES SET status = 'PAID', paidAt = NOW()
 WHERE fineID = (SELECT fineID FROM (SELECT fineID FROM FINES WHERE loanID = @loan) x)
   AND status = 'UNPAID';
SELECT ROW_COUNT() AS rows_changed;
SELECT 'PAY FINE #2 same fine (expect rows_changed = 0)' AS step;
UPDATE FINES SET status = 'PAID', paidAt = NOW()
 WHERE loanID = @loan AND status = 'UNPAID';
SELECT ROW_COUNT() AS rows_changed;

-- ---------- cleanup ----------
DELETE FROM FINES WHERE loanID IN (SELECT loanID FROM LOANS WHERE bookID = 9001);
DELETE FROM LOANS WHERE bookID = 9001;
DELETE FROM BOOKS WHERE bookID = 9001;
DELETE FROM MEMBERS WHERE memberID IN (9001, 9002);
SELECT 'CLEANUP DONE (expect 0 test rows left)' AS step;
SELECT (SELECT COUNT(*) FROM BOOKS WHERE bookID = 9001)
     + (SELECT COUNT(*) FROM MEMBERS WHERE memberID IN (9001,9002))
     + (SELECT COUNT(*) FROM LOANS WHERE bookID = 9001) AS test_rows_left;