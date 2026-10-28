-- =====================================================================
-- reports_test_setup.sql
-- Sample data for the Stage 8 report walkthrough.
-- Creates 3 books (IDs 9201-9203), 3 students (IDs 9201-9203), 7 loans,
-- 3 fines and 5 reservations. All dates are relative to today, so the
-- reports always look sensible whenever you run this.
--
-- Safe to re-run: it removes its own earlier rows first, and it touches
-- nothing outside IDs 9201-9203.
-- Run:  USE library_db;  SOURCE C:/libsql/reports_test_setup.sql;
-- Undo: SOURCE C:/libsql/reports_test_cleanup.sql;
-- =====================================================================
USE library_db;

-- 0. Remove anything left from an earlier run (children before parents).
DELETE FROM FINES        WHERE loanID IN (SELECT loanID FROM LOANS WHERE bookID BETWEEN 9201 AND 9203);
DELETE FROM RESERVATIONS WHERE bookID BETWEEN 9201 AND 9203;
DELETE FROM LOANS        WHERE bookID BETWEEN 9201 AND 9203;
DELETE FROM BOOKS        WHERE bookID BETWEEN 9201 AND 9203;
DELETE FROM MEMBERS      WHERE memberID BETWEEN 9201 AND 9203;

-- 1. Books. availableCopies already reflects the loans that stay out
--    (see step 3), so inventory is consistent from the start.
INSERT INTO BOOKS (bookID, title, author, totalCopies, availableCopies) VALUES
 (9201, 'Report Test: Data Structures',  'Test Author A', 3, 2),
 (9202, 'Report Test: Databases',        'Test Author B', 2, 0),
 (9203, 'Report Test: Operating Systems','Test Author C', 1, 0);

-- 2. Students.
INSERT INTO MEMBERS (memberID, registrationNo, name, memberType) VALUES
 (9201, 'RPT-P', 'Report Student P', 'Student'),
 (9202, 'RPT-Q', 'Report Student Q', 'Student'),
 (9203, 'RPT-R', 'Report Student R', 'Student');

-- 3. Loans: (book, member, issued N days ago, due N days ago, returned N days ago)
INSERT INTO LOANS (bookID, memberID, issueDate, dueDate, returnDate, renewalCount) VALUES
 -- returned LATE (fines below)
 (9201, 9201, DATE_SUB(CURDATE(), INTERVAL 40 DAY), DATE_SUB(CURDATE(), INTERVAL 26 DAY), DATE_SUB(CURDATE(), INTERVAL 20 DAY), 0),
 (9201, 9202, DATE_SUB(CURDATE(), INTERVAL 25 DAY), DATE_SUB(CURDATE(), INTERVAL 11 DAY), DATE_SUB(CURDATE(), INTERVAL  8 DAY), 0),
 (9201, 9203, DATE_SUB(CURDATE(), INTERVAL 60 DAY), DATE_SUB(CURDATE(), INTERVAL 46 DAY), DATE_SUB(CURDATE(), INTERVAL 40 DAY), 0);

INSERT INTO LOANS (bookID, memberID, issueDate, dueDate, renewalCount) VALUES
 -- still out; the first one is OVERDUE by 6 days
 (9202, 9201, DATE_SUB(CURDATE(), INTERVAL 20 DAY), DATE_SUB(CURDATE(), INTERVAL  6 DAY), 0),
 (9202, 9202, DATE_SUB(CURDATE(), INTERVAL  3 DAY), DATE_ADD(CURDATE(), INTERVAL 11 DAY), 0),
 (9203, 9203, DATE_SUB(CURDATE(), INTERVAL  1 DAY), DATE_ADD(CURDATE(), INTERVAL 13 DAY), 0),
 (9201, 9201, DATE_SUB(CURDATE(), INTERVAL  2 DAY), DATE_ADD(CURDATE(), INTERVAL 12 DAY), 0);

-- 4. Fines: P owes Rs 30 (6 days late), Q owes Rs 15 (3 days late),
--    R paid Rs 30 already. createdAt = the day the book came back.
INSERT INTO FINES (loanID, amount, status, createdAt)
SELECT loanID, 30, 'UNPAID', returnDate FROM LOANS
 WHERE bookID = 9201 AND memberID = 9201 AND returnDate IS NOT NULL;

INSERT INTO FINES (loanID, amount, status, createdAt)
SELECT loanID, 15, 'UNPAID', returnDate FROM LOANS
 WHERE bookID = 9201 AND memberID = 9202 AND returnDate IS NOT NULL;

INSERT INTO FINES (loanID, amount, status, createdAt, paidAt)
SELECT loanID, 30, 'PAID', returnDate, DATE_SUB(NOW(), INTERVAL 38 DAY) FROM LOANS
 WHERE bookID = 9201 AND memberID = 9203 AND returnDate IS NOT NULL;

-- 5. Reservations. Queue for 9203: P first (2 days ago), then Q (1 day ago).
INSERT INTO RESERVATIONS (bookID, memberID, reservationDate, status) VALUES
 (9203, 9201, DATE_SUB(NOW(), INTERVAL 2 DAY),  'WAITING'),
 (9203, 9202, DATE_SUB(NOW(), INTERVAL 1 DAY),  'WAITING'),
 (9202, 9203, DATE_SUB(NOW(), INTERVAL 3 HOUR), 'WAITING'),
 (9201, 9202, DATE_SUB(NOW(), INTERVAL 9 DAY),  'CANCELLED'),
 (9201, 9203, DATE_SUB(NOW(), INTERVAL 30 DAY), 'FULFILLED');

SELECT 'books' AS tbl, COUNT(*) AS n FROM BOOKS WHERE bookID BETWEEN 9201 AND 9203
UNION ALL SELECT 'members', COUNT(*) FROM MEMBERS WHERE memberID BETWEEN 9201 AND 9203
UNION ALL SELECT 'loans', COUNT(*) FROM LOANS WHERE bookID BETWEEN 9201 AND 9203
UNION ALL SELECT 'fines', COUNT(*) FROM FINES WHERE loanID IN (SELECT loanID FROM LOANS WHERE bookID BETWEEN 9201 AND 9203)
UNION ALL SELECT 'reservations', COUNT(*) FROM RESERVATIONS WHERE bookID BETWEEN 9201 AND 9203;
-- expected: books 3, members 3, loans 7, fines 3, reservations 5
