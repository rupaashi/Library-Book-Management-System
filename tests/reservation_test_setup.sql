-- =====================================================================
-- reservation_test_setup.sql
-- Creates ONE test book (1 copy) and THREE test students for the
-- Stage 6 walkthrough. Uses IDs 9101-9103, far from real data.
-- Safe to re-run: it removes its own earlier test rows first.
-- Run:  USE library_db;  SOURCE C:/libsql/reservation_test_setup.sql;
-- =====================================================================
USE library_db;

DELETE FROM FINES        WHERE loanID IN (SELECT loanID FROM LOANS WHERE bookID = 9101);
DELETE FROM RESERVATIONS WHERE bookID = 9101;
DELETE FROM LOANS        WHERE bookID = 9101;
DELETE FROM BOOKS        WHERE bookID = 9101;
DELETE FROM MEMBERS      WHERE memberID IN (9101, 9102, 9103);

INSERT INTO BOOKS (bookID, title, author, totalCopies, availableCopies)
VALUES (9101, 'Reservation Test Book', 'Test Author', 1, 1);

INSERT INTO MEMBERS (memberID, registrationNo, name, memberType)
VALUES (9101, 'RTEST-A', 'Test Student A', 'Student'),
       (9102, 'RTEST-B', 'Test Student B', 'Student'),
       (9103, 'RTEST-C', 'Test Student C', 'Student');

SELECT bookID, title, totalCopies, availableCopies FROM BOOKS WHERE bookID = 9101;
SELECT memberID, name, memberType, active FROM MEMBERS WHERE memberID BETWEEN 9101 AND 9103;
