-- =====================================================================
-- reports_test_cleanup.sql
-- Removes everything reports_test_setup.sql created (books 9201-9203,
-- students 9201-9203 and their loans, fines and reservations).
-- Touches no other rows.
-- =====================================================================
USE library_db;

DELETE FROM FINES        WHERE loanID IN (SELECT loanID FROM LOANS WHERE bookID BETWEEN 9201 AND 9203);
DELETE FROM RESERVATIONS WHERE bookID BETWEEN 9201 AND 9203;
DELETE FROM LOANS        WHERE bookID BETWEEN 9201 AND 9203;
DELETE FROM BOOKS        WHERE bookID BETWEEN 9201 AND 9203;
DELETE FROM MEMBERS      WHERE memberID BETWEEN 9201 AND 9203;

SELECT COUNT(*) AS leftover_rows FROM BOOKS WHERE bookID BETWEEN 9201 AND 9203;
