-- =====================================================================
-- 05_books_members_tables.sql
-- Recreates the teammate's BOOKS and MEMBERS tables exactly as shown by
-- SHOW CREATE TABLE on the teammate's database, so the project can be
-- set up on any computer.
--
-- SAFE TO RE-RUN: CREATE TABLE IF NOT EXISTS skips tables that already
-- exist. It never drops, alters or deletes anything.
--
-- Run order: 05_books_members_tables.sql, then 10_circulation_tables.sql
-- =====================================================================

CREATE DATABASE IF NOT EXISTS library_db;
USE library_db;

CREATE TABLE IF NOT EXISTS BOOKS (
    bookID           INT          NOT NULL,
    title            VARCHAR(100) NOT NULL,
    author           VARCHAR(100) NOT NULL,
    isbn             VARCHAR(20)  DEFAULT NULL,
    publisher        VARCHAR(100) DEFAULT NULL,
    category         VARCHAR(50)  DEFAULT NULL,
    publicationYear  INT          DEFAULT NULL,
    totalCopies      INT          NOT NULL DEFAULT 0,
    availableCopies  INT          NOT NULL DEFAULT 0,
    damagedCopies    INT          NOT NULL DEFAULT 0,
    lostCopies       INT          NOT NULL DEFAULT 0,
    active           TINYINT(1)   NOT NULL DEFAULT 1,
    PRIMARY KEY (bookID),
    UNIQUE KEY isbn (isbn)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS MEMBERS (
    memberID         INT          NOT NULL,
    registrationNo   VARCHAR(30)  NOT NULL,
    name             VARCHAR(100) NOT NULL,
    email            VARCHAR(100) DEFAULT NULL,
    phone            VARCHAR(20)  DEFAULT NULL,
    memberType       VARCHAR(20)  NOT NULL,
    active           TINYINT(1)   NOT NULL DEFAULT 1,
    PRIMARY KEY (memberID),
    UNIQUE KEY registrationNo (registrationNo),
    UNIQUE KEY email (email)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;