-- =====================================================================
-- concurrency_test.sql - the "last copy" race, done by hand with TWO
-- MySQL Command Line Client windows (call them A and B).
-- Needs edge_test_setup.sql (book 9303 has exactly 1 copy).
-- This is the same UPDATE that issueBook() runs in src/circulation.c,
-- with the "?" values filled in (book 9303, student 9301 / 9304).
-- =====================================================================

-- WINDOW A:
USE library_db;
START TRANSACTION;
UPDATE BOOKS b SET b.availableCopies = b.availableCopies - 1
WHERE b.bookID = 9303 AND b.active = 1
  AND b.availableCopies > (SELECT COUNT(*) FROM RESERVATIONS r
        WHERE r.bookID = b.bookID AND r.status = 'READY' AND r.memberID <> 9301);
-- Expect: Rows matched: 1  Changed: 1.   (do NOT commit yet)

-- WINDOW B (while A is still open):
USE library_db;
START TRANSACTION;
UPDATE BOOKS b SET b.availableCopies = b.availableCopies - 1
WHERE b.bookID = 9303 AND b.active = 1
  AND b.availableCopies > (SELECT COUNT(*) FROM RESERVATIONS r
        WHERE r.bookID = b.bookID AND r.status = 'READY' AND r.memberID <> 9304);
-- Expect: B WAITS (the row is locked by A). This is the lock in action.

-- WINDOW A:
COMMIT;

-- WINDOW B now finishes by itself. Expect: Rows matched: 0  Changed: 0
-- (availableCopies is already 0, so the WHERE is false). In the C program
-- this is "changed == 0" -> rollback -> "No copy is available right now."
ROLLBACK;

-- Check, then restore: availableCopies must be 0, never -1.
SELECT bookID, availableCopies FROM BOOKS WHERE bookID = 9303;
UPDATE BOOKS SET availableCopies = 1 WHERE bookID = 9303;
