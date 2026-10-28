-- =====================================================================
-- setup_all.sql - build the whole database on a new computer.
-- SAFE TO RE-RUN: every script uses CREATE ... IF NOT EXISTS and never
-- drops or alters existing tables.
--
-- Run from INSIDE the sql folder (so the relative names resolve):
--   cd <repo>\sql
--   mysql -u root -p
--   mysql> SOURCE setup_all.sql;
-- Result: database library_db with BOOKS, MEMBERS, LOANS, RESERVATIONS, FINES.
-- =====================================================================
SOURCE 05_books_members_tables.sql;
SOURCE 10_circulation_tables.sql;

USE library_db;
SHOW TABLES;
