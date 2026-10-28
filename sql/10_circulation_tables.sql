-- =====================================================================
-- 10_circulation_tables.sql
-- Circulation, reservations and fines tables for the Library project.
--
-- SAFE TO RE-RUN: uses CREATE TABLE IF NOT EXISTS and never DROPs or
-- ALTERs the teammate's BOOKS / MEMBERS tables.
--
-- Requires BOOKS and MEMBERS to already exist in library_db with
-- bookID INT and memberID INT as primary keys (they do).
--
-- Project rules that are NOT in SQL (kept in include/config.h later):
--   loan period = 14 days, fine = Rs 5 per overdue day,
--   max renewals = 2, reservation hold = 3 days.
-- =====================================================================

CREATE DATABASE IF NOT EXISTS library_db;
USE library_db;

-- ---------------------------------------------------------------------
-- LOANS: one row per borrowing transaction.
-- A loan is ACTIVE while returnDate IS NULL. Availability of a title is
-- stored separately in BOOKS.availableCopies (quantity-based inventory).
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS LOANS (
    loanID        INT  NOT NULL AUTO_INCREMENT,
    bookID        INT  NOT NULL,
    memberID      INT  NOT NULL,
    issueDate     DATE NOT NULL,
    dueDate       DATE NOT NULL,
    returnDate    DATE NULL,
    renewalCount  INT  NOT NULL DEFAULT 0,

    -- 1 while the loan is active, NULL once returned. MySQL UNIQUE indexes
    -- allow many NULLs, so this lets us enforce "at most one ACTIVE loan
    -- of a title per student" while keeping unlimited history.
    activeFlag    TINYINT GENERATED ALWAYS AS (IF(returnDate IS NULL, 1, NULL)) STORED,

    PRIMARY KEY (loanID),

    CONSTRAINT fk_loans_book
        FOREIGN KEY (bookID)   REFERENCES BOOKS (bookID),
    CONSTRAINT fk_loans_member
        FOREIGN KEY (memberID) REFERENCES MEMBERS (memberID),

    CONSTRAINT chk_loans_due     CHECK (dueDate >= issueDate),
    CONSTRAINT chk_loans_return  CHECK (returnDate IS NULL OR returnDate >= issueDate),
    CONSTRAINT chk_loans_renew   CHECK (renewalCount >= 0),

    UNIQUE KEY uq_loans_one_active (bookID, memberID, activeFlag),
    INDEX idx_loans_member (memberID, returnDate),
    INDEX idx_loans_overdue (returnDate, dueDate)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- RESERVATIONS: persistent FIFO queue per book title.
-- Queue order = reservationDate, then reservationID as tie-breaker.
-- Statuses: WAITING -> READY -> FULFILLED, or CANCELLED / EXPIRED.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS RESERVATIONS (
    reservationID    INT      NOT NULL AUTO_INCREMENT,
    bookID           INT      NOT NULL,
    memberID         INT      NOT NULL,
    reservationDate  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    status           ENUM('WAITING','READY','FULFILLED','CANCELLED','EXPIRED')
                     NOT NULL DEFAULT 'WAITING',
    expiryDate       DATE     NULL,

    -- 1 while WAITING or READY, NULL otherwise: same trick as LOANS, so
    -- a student cannot hold two live reservations for one title.
    activeFlag       TINYINT GENERATED ALWAYS AS
                     (IF(status IN ('WAITING','READY'), 1, NULL)) STORED,

    PRIMARY KEY (reservationID),

    CONSTRAINT fk_res_book
        FOREIGN KEY (bookID)   REFERENCES BOOKS (bookID),
    CONSTRAINT fk_res_member
        FOREIGN KEY (memberID) REFERENCES MEMBERS (memberID),

    UNIQUE KEY uq_res_one_active (bookID, memberID, activeFlag),
    -- Supports "oldest WAITING reservation for this book" lookups.
    INDEX idx_res_queue (bookID, status, reservationDate, reservationID),
    INDEX idx_res_member (memberID, status)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- FINES: at most one fine per loan (UNIQUE loanID makes retries safe).
-- Full payment only: UNPAID or PAID, paidAt set exactly when PAID.
-- DECIMAL (not FLOAT) so money is exact.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS FINES (
    fineID     INT           NOT NULL AUTO_INCREMENT,
    loanID     INT           NOT NULL,
    amount     DECIMAL(8,2)  NOT NULL,
    status     ENUM('UNPAID','PAID') NOT NULL DEFAULT 'UNPAID',
    createdAt  DATE          NOT NULL DEFAULT (CURRENT_DATE),
    paidAt     DATETIME      NULL,

    PRIMARY KEY (fineID),
    UNIQUE KEY uq_fines_loan (loanID),

    CONSTRAINT fk_fines_loan
        FOREIGN KEY (loanID) REFERENCES LOANS (loanID),

    CONSTRAINT chk_fines_amount CHECK (amount > 0),
    CONSTRAINT chk_fines_paid   CHECK (
        (status = 'PAID'   AND paidAt IS NOT NULL) OR
        (status = 'UNPAID' AND paidAt IS NULL)
    ),

    INDEX idx_fines_status (status)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------------
-- OPTIONAL safety net on the teammate's table (NOT run automatically).
-- BOOKS currently allows availableCopies to go negative. Run this only
-- after agreeing with your teammate, and only if it succeeds on your
-- existing data (it fails if any row already violates it):
--
-- ALTER TABLE BOOKS
--   ADD CONSTRAINT chk_books_avail
--   CHECK (availableCopies >= 0 AND availableCopies <= totalCopies);
-- ---------------------------------------------------------------------