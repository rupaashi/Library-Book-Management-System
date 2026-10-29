-- =====================================================================
-- edge_test_setup.sql - sample data for the Stage 10 edge-case tests.
-- Books 9301-9303, members 9301-9304. Safe to re-run; touches nothing else.
-- Run:  USE library_db;  SOURCE C:/libsql/edge_test_setup.sql;
-- Undo: SOURCE C:/libsql/edge_test_cleanup.sql;
-- =====================================================================
USE library_db;

DELETE FROM FINES        WHERE loanID IN (SELECT loanID FROM LOANS WHERE bookID BETWEEN 9301 AND 9303);
DELETE FROM RESERVATIONS WHERE bookID BETWEEN 9301 AND 9303;
DELETE FROM LOANS        WHERE bookID BETWEEN 9301 AND 9303;
DELETE FROM BOOKS        WHERE bookID BETWEEN 9301 AND 9303;
DELETE FROM MEMBERS      WHERE memberID BETWEEN 9301 AND 9304;

INSERT INTO BOOKS (bookID, title, author, totalCopies, availableCopies, active) VALUES
 (9301, 'Edge Test Book',     'Test Author', 2, 2, 1),   -- normal, 2 copies
 (9302, 'Edge Inactive Book', 'Test Author', 1, 1, 0),   -- deactivated title
 (9303, 'Edge Last Copy',     'Test Author', 1, 1, 1);   -- exactly 1 copy

INSERT INTO MEMBERS (memberID, registrationNo, name, memberType, active) VALUES
 (9301, 'EDGE-A', 'Edge Student A',        'Student', 1),
 (9302, 'EDGE-F', 'Edge Faculty',          'Faculty', 1), -- not a student
 (9303, 'EDGE-I', 'Edge Inactive Student', 'Student', 0), -- deactivated
 (9304, 'EDGE-B', 'Edge Student B',        'Student', 1);

SELECT bookID, title, totalCopies, availableCopies, active FROM BOOKS WHERE bookID BETWEEN 9301 AND 9303;
SELECT memberID, name, memberType, active FROM MEMBERS WHERE memberID BETWEEN 9301 AND 9304;
