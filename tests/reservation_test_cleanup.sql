-- =====================================================================
-- reservation_test_cleanup.sql
-- Removes everything reservation_test_setup.sql and the walkthrough
-- created (test book 9101, students 9101-9103 and their loans, fines
-- and reservations). Touches no other rows.
-- =====================================================================
USE library_db;

DELETE FROM FINES        WHERE loanID IN (SELECT loanID FROM LOANS WHERE bookID = 9101);
DELETE FROM RESERVATIONS WHERE bookID = 9101;
DELETE FROM LOANS        WHERE bookID = 9101;
DELETE FROM BOOKS        WHERE bookID = 9101;
DELETE FROM MEMBERS      WHERE memberID IN (9101, 9102, 9103);

SELECT COUNT(*) AS leftover_rows FROM BOOKS WHERE bookID = 9101;
