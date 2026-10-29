-- =====================================================================
-- edge_test_cleanup.sql - removes everything edge_test_setup.sql and the
-- edge-case walkthrough created (books 9301-9303, members 9301-9304 and
-- their loans, fines, reservations). Touches no other rows.
-- =====================================================================
USE library_db;

DELETE FROM FINES        WHERE loanID IN (SELECT loanID FROM LOANS WHERE bookID BETWEEN 9301 AND 9303);
DELETE FROM RESERVATIONS WHERE bookID BETWEEN 9301 AND 9303;
DELETE FROM LOANS        WHERE bookID BETWEEN 9301 AND 9303;
DELETE FROM BOOKS        WHERE bookID BETWEEN 9301 AND 9303;
DELETE FROM MEMBERS      WHERE memberID BETWEEN 9301 AND 9304;

SELECT COUNT(*) AS leftover_rows FROM BOOKS WHERE bookID BETWEEN 9301 AND 9303;
