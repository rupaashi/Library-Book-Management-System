"""
library_api.py - web API for circulation, reservations, fines and reports.

WHY THIS FILE EXISTS
  The Flask app (app.py) only offered books and members. Issuing, returning,
  renewing, reservations, fines and reports exist only in the C console
  program, which a browser cannot call. This blueprint exposes the SAME rules
  over HTTP, in the same order, with the same SQL ideas as the C modules:
  one transaction per action, FOR UPDATE locks, book row locked before
  reservation rows, a guarded stock UPDATE, an in-memory FIFO queue rebuilt
  from the RESERVATIONS table (the table is the source of truth).

  Policy numbers (loan days, fine rate, ...) are read from include/config.h so
  the C program and the API cannot disagree.

CONFIGURATION (environment variables, never in code)
  LIB_DB_HOST (localhost)  LIB_DB_PORT (3306)  LIB_DB_USER (root)
  LIB_DB_PASS (required)   LIB_DB_NAME (library_db)
"""
import os
import re
from collections import deque
from contextlib import contextmanager
from datetime import date, datetime
from decimal import Decimal
from pathlib import Path

import mysql.connector
from flask import Blueprint, current_app, jsonify, request
from mysql.connector import Error

api = Blueprint("library_api", __name__, url_prefix="/api")


# ---------------------------------------------------------------- policy
def _read_policy():
    """Read #define values from include/config.h (single source of truth)."""
    defaults = {"LOAN_PERIOD_DAYS": 14, "FINE_RATE_PER_DAY": 5,
                "MAX_RENEWALS": 2, "RESERVATION_HOLD_DAYS": 3}
    header = Path(__file__).resolve().parent.parent / "include" / "config.h"
    try:
        text = header.read_text(errors="ignore")
        for name in defaults:
            m = re.search(r"#define\s+%s\s+(\d+)" % name, text)
            if m:
                defaults[name] = int(m.group(1))
    except OSError:
        pass                      # deployed without the C headers: use defaults
    return defaults


POLICY = _read_policy()
LOAN_DAYS = POLICY["LOAN_PERIOD_DAYS"]
FINE_RATE = POLICY["FINE_RATE_PER_DAY"]
MAX_RENEWALS = POLICY["MAX_RENEWALS"]
HOLD_DAYS = POLICY["RESERVATION_HOLD_DAYS"]


# ---------------------------------------------------------------- plumbing
class ApiError(Exception):
    def __init__(self, message, status=400):
        super().__init__(message)
        self.message = message
        self.status = status


@api.errorhandler(ApiError)
def _api_error(err):
    return jsonify({"error": err.message}), err.status


@api.errorhandler(Error)
def _db_error(err):
    # Details go to the server log, not to the browser.
    current_app.logger.error("Database error: %s", err)
    if getattr(err, "errno", None) == 1045:
        return jsonify({"error": "Database login failed. Check LIB_DB_PASS."}), 500
    return jsonify({"error": "Database error. Is MySQL running and library_db set up?"}), 500


def _connect():
    return mysql.connector.connect(
        host=os.environ.get("LIB_DB_HOST", "localhost"),
        port=int(os.environ.get("LIB_DB_PORT", "3306")),
        user=os.environ.get("LIB_DB_USER", "root"),
        password=os.environ.get("LIB_DB_PASS", ""),
        database=os.environ.get("LIB_DB_NAME", "library_db"),
    )


def _clean(value):
    """JSON-friendly values: dates as ISO text, money as exact text."""
    if isinstance(value, datetime):
        return value.strftime("%Y-%m-%d %H:%M:%S")
    if isinstance(value, date):
        return value.isoformat()
    if isinstance(value, Decimal):
        return format(value, "f")
    return value


def _rows(rows):
    return [{k: _clean(v) for k, v in row.items()} for row in rows]


@contextmanager
def transaction():
    """One database transaction: commit on success, roll back on any error."""
    conn = _connect()
    cur = conn.cursor(dictionary=True, buffered=True)
    try:
        conn.start_transaction()
        yield cur
        conn.commit()
    except BaseException:
        conn.rollback()
        raise
    finally:
        cur.close()
        conn.close()


def read(sql, params=()):
    """Run one SELECT outside a transaction and return clean rows."""
    conn = _connect()
    try:
        cur = conn.cursor(dictionary=True, buffered=True)
        cur.execute(sql, params)
        data = _rows(cur.fetchall())
        cur.close()
        return data
    finally:
        conn.close()


def read_one(sql, params=()):
    data = read(sql, params)
    return data[0] if data else None


def body():
    return request.get_json(silent=True) or {}


def int_field(data, name, label):
    try:
        value = int(data.get(name))
    except (TypeError, ValueError):
        raise ApiError("%s must be a whole number." % label)
    if value <= 0:
        raise ApiError("%s must be a positive number." % label)
    return value


def int_arg(name, default=0, low=0, high=None):
    try:
        value = int(request.args.get(name, default))
    except ValueError:
        value = default
    value = max(low, value)
    return min(value, high) if high is not None else value


# ---------------------------------------------------------------- shared rules
def check_member_eligible(cur, member_id):
    """Who may borrow or reserve? Mirrors checkMemberEligible() in C."""
    cur.execute("SELECT active, memberType FROM MEMBERS WHERE memberID = %s",
                (member_id,))
    m = cur.fetchone()
    if not m:
        raise ApiError("Member %d does not exist." % member_id, 404)
    if not m["active"]:
        raise ApiError("Member %d is inactive." % member_id, 409)
    if m["memberType"] != "Student":
        raise ApiError("Member %d is not a student. Only students can borrow."
                       % member_id, 409)
    cur.execute("SELECT COUNT(*) AS n, COALESCE(SUM(f.amount), 0) AS total "
                "FROM FINES f JOIN LOANS l ON l.loanID = f.loanID "
                "WHERE l.memberID = %s AND f.status = 'UNPAID'", (member_id,))
    f = cur.fetchone()
    if f["n"] > 0:
        raise ApiError("Member %d has %d unpaid fine(s) totalling Rs %s. "
                       "Pay them first." % (member_id, f["n"], format(f["total"], "f")), 409)


def eligible_quietly(cur, member_id):
    try:
        check_member_eligible(cur, member_id)
        return True
    except ApiError:
        return False


def promote_next(cur, book_id):
    """Give free copies to the oldest eligible WAITING reservations.
    Must run inside a transaction. Returns the promoted reservation IDs."""
    cur.execute("SELECT availableCopies FROM BOOKS "
                "WHERE bookID = %s AND active = 1 FOR UPDATE", (book_id,))
    book = cur.fetchone()
    if not book:
        return []
    cur.execute("SELECT COUNT(*) AS n FROM RESERVATIONS "
                "WHERE bookID = %s AND status = 'READY' FOR UPDATE", (book_id,))
    free = book["availableCopies"] - cur.fetchone()["n"]
    if free <= 0:
        return []

    # FIFO queue, loaded fresh from the table (same idea as queue.c).
    cur.execute("SELECT reservationID, memberID FROM RESERVATIONS "
                "WHERE bookID = %s AND status = 'WAITING' "
                "ORDER BY reservationDate, reservationID FOR UPDATE", (book_id,))
    queue = deque(cur.fetchall())

    promoted = []
    while free > 0 and queue:
        nxt = queue.popleft()
        if not eligible_quietly(cur, nxt["memberID"]):
            continue                      # skipped, but keeps its place
        cur.execute("UPDATE RESERVATIONS SET status = 'READY', "
                    "expiryDate = DATE_ADD(CURDATE(), INTERVAL %s DAY) "
                    "WHERE reservationID = %s AND status = 'WAITING'",
                    (HOLD_DAYS, nxt["reservationID"]))
        if cur.rowcount == 1:
            promoted.append(nxt["reservationID"])
            free -= 1
    return promoted


def describe_ready(cur, reservation_ids):
    """Who was told their book is ready? (for the success message)"""
    out = []
    for rid in reservation_ids:
        cur.execute("SELECT r.reservationID, m.name AS memberName, b.title, "
                    "r.expiryDate FROM RESERVATIONS r "
                    "JOIN MEMBERS m ON m.memberID = r.memberID "
                    "JOIN BOOKS b ON b.bookID = r.bookID "
                    "WHERE r.reservationID = %s", (rid,))
        out.extend(_rows(cur.fetchall()))
    return out


# ---------------------------------------------------------------- config / dashboard
@api.get("/config")
def get_config():
    return jsonify({"loanPeriodDays": LOAN_DAYS, "finePerDay": FINE_RATE,
                    "maxRenewals": MAX_RENEWALS, "holdDays": HOLD_DAYS})


@api.get("/dashboard")
def dashboard():
    books = read_one("SELECT COUNT(*) AS titles, COALESCE(SUM(totalCopies),0) AS totalCopies, "
                     "COALESCE(SUM(availableCopies),0) AS availableCopies "
                     "FROM BOOKS WHERE active = 1")
    loans = read_one("SELECT COUNT(*) AS issued, "
                     "COALESCE(SUM(dueDate < CURDATE()),0) AS overdue "
                     "FROM LOANS WHERE returnDate IS NULL")
    members = read_one("SELECT COUNT(*) AS n FROM MEMBERS "
                       "WHERE active = 1 AND memberType = 'Student'")
    res = read_one("SELECT COALESCE(SUM(status='WAITING'),0) AS waiting, "
                   "COALESCE(SUM(status='READY'),0) AS ready "
                   "FROM RESERVATIONS WHERE status IN ('WAITING','READY')")
    fines = read_one("SELECT COUNT(*) AS n, COALESCE(SUM(amount),0) AS total "
                     "FROM FINES WHERE status = 'UNPAID'")
    recent = read(
        "(SELECT 'Issued' AS event, l.issueDate AS eventDate, l.loanID, b.title, m.name AS memberName "
        " FROM LOANS l JOIN BOOKS b ON b.bookID = l.bookID JOIN MEMBERS m ON m.memberID = l.memberID) "
        "UNION ALL "
        "(SELECT 'Returned', l.returnDate, l.loanID, b.title, m.name "
        " FROM LOANS l JOIN BOOKS b ON b.bookID = l.bookID JOIN MEMBERS m ON m.memberID = l.memberID "
        " WHERE l.returnDate IS NOT NULL) "
        "ORDER BY eventDate DESC, loanID DESC LIMIT 8")
    return jsonify({
        "titles": int(books["titles"]), "totalCopies": int(books["totalCopies"]),
        "availableCopies": int(books["availableCopies"]),
        "issuedLoans": int(loans["issued"]), "overdueLoans": int(loans["overdue"]),
        "activeStudents": int(members["n"]),
        "waitingReservations": int(res["waiting"]), "readyReservations": int(res["ready"]),
        "unpaidFines": int(fines["n"]), "unpaidAmount": fines["total"],
        "recent": recent,
    })


# ---------------------------------------------------------------- books / members
@api.patch("/books/<int:book_id>")
def edit_book(book_id):
    data = body()
    title = str(data.get("title") or "").strip()
    author = str(data.get("author") or "").strip()
    category = str(data.get("category") or "").strip()
    isbn = str(data.get("isbn") or "").strip() or None
    publisher = str(data.get("publisher") or "").strip() or None
    if not title or not author or not category:
        raise ApiError("Title, author and category are required.")
    with transaction() as cur:
        cur.execute("SELECT bookID FROM BOOKS WHERE bookID = %s FOR UPDATE", (book_id,))
        if not cur.fetchone():
            raise ApiError("Book %d does not exist." % book_id, 404)
        try:
            cur.execute("UPDATE BOOKS SET title=%s, author=%s, category=%s, isbn=%s, "
                        "publisher=%s WHERE bookID=%s",
                        (title, author, category, isbn, publisher, book_id))
        except Error as err:
            if getattr(err, "errno", None) == 1062:
                raise ApiError("Another book already uses this ISBN.", 409)
            raise
    return jsonify({"message": "Book %d updated." % book_id})


@api.post("/books/<int:book_id>/deactivate")
def deactivate_book(book_id):
    with transaction() as cur:
        cur.execute("UPDATE BOOKS SET active = 0 WHERE bookID = %s AND active = 1", (book_id,))
        if cur.rowcount == 0:
            raise ApiError("Book %d does not exist or is already inactive." % book_id, 404)
    return jsonify({"message": "Book %d deactivated. Its history is kept." % book_id})


@api.post("/members/<int:member_id>/deactivate")
def deactivate_member(member_id):
    with transaction() as cur:
        cur.execute("UPDATE MEMBERS SET active = 0 WHERE memberID = %s AND active = 1",
                    (member_id,))
        if cur.rowcount == 0:
            raise ApiError("Member %d does not exist or is already inactive." % member_id, 404)
    return jsonify({"message": "Member %d deactivated. Their history is kept." % member_id})


# ---------------------------------------------------------------- loans
LOAN_SELECT = (
    "SELECT l.loanID, l.bookID, b.title, l.memberID, m.name AS memberName, "
    "m.registrationNo, l.issueDate, l.dueDate, l.returnDate, l.renewalCount, "
    "CASE WHEN l.returnDate IS NOT NULL THEN 'RETURNED' "
    "     WHEN l.dueDate < CURDATE() THEN 'OVERDUE' ELSE 'ACTIVE' END AS state, "
    "CASE WHEN l.returnDate IS NULL AND l.dueDate < CURDATE() "
    "     THEN DATEDIFF(CURDATE(), l.dueDate) ELSE 0 END AS daysOverdue, "
    "f.amount AS fineAmount, f.status AS fineStatus "
    "FROM LOANS l JOIN BOOKS b ON b.bookID = l.bookID "
    "JOIN MEMBERS m ON m.memberID = l.memberID "
    "LEFT JOIN FINES f ON f.loanID = l.loanID ")


@api.get("/loans")
def list_loans():
    status = request.args.get("status", "active")
    where, params = [], []
    if status == "active":
        where.append("l.returnDate IS NULL")
    elif status == "overdue":
        where.append("l.returnDate IS NULL AND l.dueDate < CURDATE()")
    elif status == "returned":
        where.append("l.returnDate IS NOT NULL")
    elif status != "all":
        raise ApiError("status must be active, overdue, returned or all.")
    if request.args.get("memberID"):
        where.append("l.memberID = %s")
        params.append(int_arg("memberID", 0))
    sql = LOAN_SELECT + ("WHERE " + " AND ".join(where) + " " if where else "")
    sql += ("ORDER BY l.dueDate, l.loanID " if status in ("active", "overdue")
            else "ORDER BY l.issueDate DESC, l.loanID DESC ") + "LIMIT 1000"
    loans = read(sql, tuple(params))
    for loan in loans:
        loan["fineIfReturnedToday"] = loan["daysOverdue"] * FINE_RATE
    return jsonify(loans)


@api.post("/loans")
def issue_book():
    data = body()
    member_id = int_field(data, "memberID", "Student")
    book_id = int_field(data, "bookID", "Book")
    with transaction() as cur:
        check_member_eligible(cur, member_id)
        cur.execute("SELECT active FROM BOOKS WHERE bookID = %s", (book_id,))
        book = cur.fetchone()
        if not book:
            raise ApiError("Book %d does not exist." % book_id, 404)
        if not book["active"]:
            raise ApiError("Book %d is inactive and cannot be issued." % book_id, 409)
        cur.execute("SELECT COUNT(*) AS n FROM LOANS WHERE bookID=%s AND memberID=%s "
                    "AND returnDate IS NULL FOR UPDATE", (book_id, member_id))
        if cur.fetchone()["n"] > 0:
            raise ApiError("Member %d already has this book on loan." % member_id, 409)
        # One guarded UPDATE decides the race for the last copy; copies held
        # READY for other students are not available to this student.
        cur.execute("UPDATE BOOKS b SET b.availableCopies = b.availableCopies - 1 "
                    "WHERE b.bookID = %s AND b.active = 1 AND b.availableCopies > "
                    "(SELECT COUNT(*) FROM RESERVATIONS r WHERE r.bookID = b.bookID "
                    " AND r.status = 'READY' AND r.memberID <> %s)", (book_id, member_id))
        if cur.rowcount == 0:
            raise ApiError("No copy is available right now. You can reserve this book.", 409)
        cur.execute("INSERT INTO LOANS (bookID, memberID, issueDate, dueDate) "
                    "VALUES (%s, %s, CURDATE(), DATE_ADD(CURDATE(), INTERVAL %s DAY))",
                    (book_id, member_id, LOAN_DAYS))
        loan_id = cur.lastrowid
        cur.execute("UPDATE RESERVATIONS SET status = 'FULFILLED' WHERE bookID = %s "
                    "AND memberID = %s AND status IN ('WAITING','READY')", (book_id, member_id))
        cur.execute("SELECT dueDate FROM LOANS WHERE loanID = %s", (loan_id,))
        due = _clean(cur.fetchone()["dueDate"])
    return jsonify({"message": "Book issued. Loan %d is due on %s." % (loan_id, due),
                    "loanID": loan_id, "dueDate": due}), 201


@api.post("/loans/<int:loan_id>/return")
def return_book(loan_id):
    with transaction() as cur:
        cur.execute("SELECT bookID, returnDate IS NOT NULL AS returned, "
                    "GREATEST(DATEDIFF(CURDATE(), dueDate), 0) AS daysLate "
                    "FROM LOANS WHERE loanID = %s FOR UPDATE", (loan_id,))
        loan = cur.fetchone()
        if not loan:
            raise ApiError("Loan not found.", 404)
        if loan["returned"]:
            raise ApiError("This loan was already returned.", 409)
        cur.execute("UPDATE LOANS SET returnDate = CURDATE() "
                    "WHERE loanID = %s AND returnDate IS NULL", (loan_id,))
        if cur.rowcount == 0:
            raise ApiError("This loan was already returned.", 409)
        cur.execute("UPDATE BOOKS SET availableCopies = availableCopies + 1 "
                    "WHERE bookID = %s AND availableCopies < totalCopies", (loan["bookID"],))
        promoted = promote_next(cur, loan["bookID"])
        fine = None
        if loan["daysLate"] > 0:
            fine = loan["daysLate"] * FINE_RATE
            # UNIQUE(loanID): a retry can never create a second fine.
            cur.execute("INSERT INTO FINES (loanID, amount) VALUES (%s, %s) "
                        "ON DUPLICATE KEY UPDATE fineID = fineID", (loan_id, fine))
        ready = describe_ready(cur, promoted)
    msg = "Book returned."
    if fine is not None:
        msg = "Returned %d day(s) late. Fine recorded: Rs %.2f (unpaid)." % (loan["daysLate"], fine)
    return jsonify({"message": msg, "daysLate": loan["daysLate"],
                    "fineAmount": None if fine is None else "%.2f" % fine,
                    "readyReservations": ready})


@api.post("/loans/<int:loan_id>/renew")
def renew_loan(loan_id):
    with transaction() as cur:
        cur.execute("SELECT bookID, memberID, renewalCount, returnDate IS NOT NULL AS returned, "
                    "dueDate < CURDATE() AS overdue FROM LOANS WHERE loanID = %s FOR UPDATE",
                    (loan_id,))
        loan = cur.fetchone()
        if not loan:
            raise ApiError("Loan not found.", 404)
        if loan["returned"]:
            raise ApiError("This loan was already returned.", 409)
        if loan["overdue"]:
            raise ApiError("This loan is overdue and cannot be renewed. Please return it.", 409)
        if loan["renewalCount"] >= MAX_RENEWALS:
            raise ApiError("This loan has reached the limit of %d renewals." % MAX_RENEWALS, 409)
        check_member_eligible(cur, loan["memberID"])
        cur.execute("SELECT COUNT(*) AS n FROM RESERVATIONS WHERE bookID = %s "
                    "AND status IN ('WAITING','READY') AND memberID <> %s FOR UPDATE",
                    (loan["bookID"], loan["memberID"]))
        if cur.fetchone()["n"] > 0:
            raise ApiError("Another student has reserved this book, so it cannot be renewed.", 409)
        cur.execute("UPDATE LOANS SET dueDate = DATE_ADD(CURDATE(), INTERVAL %s DAY), "
                    "renewalCount = renewalCount + 1 WHERE loanID = %s", (LOAN_DAYS, loan_id))
        cur.execute("SELECT dueDate, renewalCount FROM LOANS WHERE loanID = %s", (loan_id,))
        row = cur.fetchone()
    return jsonify({"message": "Loan renewed. New due date: %s (renewal %d of %d)."
                    % (_clean(row["dueDate"]), row["renewalCount"], MAX_RENEWALS)})


# ---------------------------------------------------------------- reservations
@api.get("/reservations")
def list_reservations():
    status = request.args.get("status", "live")
    if status == "live":
        cond = "WHERE r.status IN ('WAITING','READY') "
    elif status == "all":
        cond = ""
    else:
        raise ApiError("status must be live or all.")
    rows = read(
        "SELECT r.reservationID, r.bookID, b.title, r.memberID, m.name AS memberName, "
        "r.reservationDate, r.status, r.expiryDate, "
        "CASE WHEN r.status = 'WAITING' THEN ROW_NUMBER() OVER "
        "  (PARTITION BY r.bookID, r.status ORDER BY r.reservationDate, r.reservationID) "
        "END AS queuePosition "
        "FROM RESERVATIONS r JOIN BOOKS b ON b.bookID = r.bookID "
        "JOIN MEMBERS m ON m.memberID = r.memberID " + cond +
        "ORDER BY r.reservationDate, r.reservationID LIMIT 1000")
    return jsonify(rows)


@api.post("/reservations")
def place_reservation():
    data = body()
    member_id = int_field(data, "memberID", "Student")
    book_id = int_field(data, "bookID", "Book")
    with transaction() as cur:
        cur.execute("SELECT active, availableCopies FROM BOOKS WHERE bookID = %s FOR UPDATE",
                    (book_id,))
        book = cur.fetchone()
        if not book:
            raise ApiError("Book %d does not exist." % book_id, 404)
        if not book["active"]:
            raise ApiError("Book %d is inactive and cannot be reserved." % book_id, 409)
        check_member_eligible(cur, member_id)
        cur.execute("SELECT COUNT(*) AS n FROM LOANS WHERE bookID = %s AND memberID = %s "
                    "AND returnDate IS NULL", (book_id, member_id))
        if cur.fetchone()["n"] > 0:
            raise ApiError("Member %d already has this book on loan." % member_id, 409)
        cur.execute("SELECT COUNT(*) AS n FROM RESERVATIONS WHERE bookID = %s AND memberID = %s "
                    "AND status IN ('WAITING','READY') FOR UPDATE", (book_id, member_id))
        if cur.fetchone()["n"] > 0:
            raise ApiError("Member %d already has an active reservation for this book."
                           % member_id, 409)
        # Reserving only makes sense when nothing can be borrowed right now.
        cur.execute("SELECT COUNT(*) AS n FROM RESERVATIONS WHERE bookID = %s "
                    "AND status = 'READY' FOR UPDATE", (book_id,))
        if book["availableCopies"] - cur.fetchone()["n"] > 0:
            raise ApiError("A copy of book %d is available now. Borrow it instead of reserving."
                           % book_id, 409)
        try:
            cur.execute("INSERT INTO RESERVATIONS (bookID, memberID, reservationDate, status) "
                        "VALUES (%s, %s, NOW(), 'WAITING')", (book_id, member_id))
        except Error as err:
            if getattr(err, "errno", None) == 1062:
                raise ApiError("Member %d already has an active reservation for this book."
                               % member_id, 409)
            raise
        rid = cur.lastrowid
        cur.execute("SELECT COUNT(*) AS pos FROM RESERVATIONS WHERE bookID = %s "
                    "AND status = 'WAITING' AND (reservationDate < (SELECT reservationDate "
                    "FROM RESERVATIONS WHERE reservationID = %s) OR (reservationDate = "
                    "(SELECT reservationDate FROM RESERVATIONS WHERE reservationID = %s) "
                    "AND reservationID <= %s))", (book_id, rid, rid, rid))
        pos = cur.fetchone()["pos"]
    return jsonify({"message": "Reservation %d placed. Position in queue: %d." % (rid, pos),
                    "reservationID": rid, "position": pos}), 201


@api.post("/reservations/<int:rid>/cancel")
def cancel_reservation(rid):
    with transaction() as cur:
        cur.execute("SELECT bookID FROM RESERVATIONS WHERE reservationID = %s", (rid,))
        row = cur.fetchone()
        if not row:
            raise ApiError("Reservation not found.", 404)
        book_id = row["bookID"]
        cur.execute("SELECT bookID FROM BOOKS WHERE bookID = %s FOR UPDATE", (book_id,))  # lock order
        cur.execute("SELECT status FROM RESERVATIONS WHERE reservationID = %s FOR UPDATE", (rid,))
        row = cur.fetchone()
        if not row:
            raise ApiError("Reservation not found.", 404)
        if row["status"] not in ("WAITING", "READY"):
            raise ApiError("Reservation %d is already %s and cannot be cancelled."
                           % (rid, row["status"]), 409)
        was_ready = row["status"] == "READY"
        cur.execute("UPDATE RESERVATIONS SET status = 'CANCELLED' WHERE reservationID = %s "
                    "AND status IN ('WAITING','READY')", (rid,))
        if cur.rowcount == 0:
            raise ApiError("Reservation could not be cancelled.", 409)
        ready = describe_ready(cur, promote_next(cur, book_id)) if was_ready else []
    return jsonify({"message": "Reservation %d cancelled." % rid, "readyReservations": ready})


@api.post("/reservations/expire")
def expire_reservations():
    """Release READY holds past their expiry date (each in its own transaction)."""
    due = read("SELECT reservationID, bookID FROM RESERVATIONS "
               "WHERE status = 'READY' AND expiryDate < CURDATE() "
               "ORDER BY expiryDate, reservationID")
    expired, ready = 0, []
    for item in due:
        with transaction() as cur:
            cur.execute("SELECT bookID FROM BOOKS WHERE bookID = %s FOR UPDATE", (item["bookID"],))
            cur.execute("UPDATE RESERVATIONS SET status = 'EXPIRED' WHERE reservationID = %s "
                        "AND status = 'READY' AND expiryDate < CURDATE()", (item["reservationID"],))
            if cur.rowcount == 0:
                continue                      # collected in the meantime
            expired += 1
            ready.extend(describe_ready(cur, promote_next(cur, item["bookID"])))
    return jsonify({"expired": expired, "readyReservations": ready,
                    "message": "%d reservation(s) expired." % expired})


# ---------------------------------------------------------------- fines
@api.get("/fines")
def list_fines():
    status = request.args.get("status", "unpaid")
    cond = {"unpaid": "WHERE f.status = 'UNPAID' ", "paid": "WHERE f.status = 'PAID' ",
            "all": ""}.get(status)
    if cond is None:
        raise ApiError("status must be unpaid, paid or all.")
    rows = read(
        "SELECT f.fineID, f.loanID, l.memberID, m.name AS memberName, b.title, "
        "f.amount, f.status, f.createdAt, f.paidAt "
        "FROM FINES f JOIN LOANS l ON l.loanID = f.loanID "
        "JOIN MEMBERS m ON m.memberID = l.memberID JOIN BOOKS b ON b.bookID = l.bookID " + cond +
        "ORDER BY f.status = 'PAID', f.createdAt DESC, f.fineID DESC LIMIT 1000")
    return jsonify(rows)


@api.post("/fines/<int:fine_id>/pay")
def pay_fine(fine_id):
    with transaction() as cur:
        cur.execute("SELECT status, amount, paidAt FROM FINES WHERE fineID = %s FOR UPDATE", (fine_id,))
        fine = cur.fetchone()
        if not fine:
            raise ApiError("Fine not found.", 404)
        if fine["status"] == "PAID":
            raise ApiError("Fine %d was already paid on %s." % (fine_id, _clean(fine["paidAt"])), 409)
        # Only an UNPAID row matches, so a double payment changes nothing.
        cur.execute("UPDATE FINES SET status = 'PAID', paidAt = NOW() "
                    "WHERE fineID = %s AND status = 'UNPAID'", (fine_id,))
        if cur.rowcount == 0:
            raise ApiError("Fine %d was already paid." % fine_id, 409)
    return jsonify({"message": "Fine %d paid in full: Rs %s." % (fine_id, format(fine["amount"], "f"))})


# ---------------------------------------------------------------- reports
@api.get("/reports/overdue")
def report_overdue():
    rows = read(
        "SELECT l.loanID, m.name AS memberName, m.registrationNo, b.title, l.issueDate, "
        "l.dueDate, DATEDIFF(CURDATE(), l.dueDate) AS daysOverdue, "
        "DATEDIFF(CURDATE(), l.dueDate) * %s AS fineIfReturnedToday "
        "FROM LOANS l JOIN BOOKS b ON b.bookID = l.bookID "
        "JOIN MEMBERS m ON m.memberID = l.memberID "
        "WHERE l.returnDate IS NULL AND l.dueDate < CURDATE() "
        "ORDER BY l.dueDate, l.loanID", (FINE_RATE,))
    return jsonify({"rows": rows})


@api.get("/reports/history")
def report_history():
    member_id = int_arg("memberID", 0)
    days = int_arg("days", 0)
    if member_id <= 0:
        raise ApiError("Choose a student.")
    member = read_one("SELECT name, registrationNo FROM MEMBERS WHERE memberID = %s", (member_id,))
    if not member:
        raise ApiError("Member %d does not exist." % member_id, 404)
    window = "AND (%s <= 0 OR l.issueDate >= DATE_SUB(CURDATE(), INTERVAL %s DAY)) "
    rows = read(
        "SELECT l.loanID, b.title, l.issueDate, l.dueDate, l.returnDate, "
        "CASE WHEN l.returnDate IS NOT NULL THEN 'RETURNED' "
        "     WHEN l.dueDate < CURDATE() THEN 'OVERDUE' ELSE 'ACTIVE' END AS state, "
        "f.amount AS fineAmount, f.status AS fineStatus "
        "FROM LOANS l JOIN BOOKS b ON b.bookID = l.bookID "
        "LEFT JOIN FINES f ON f.loanID = l.loanID WHERE l.memberID = %s " + window +
        "ORDER BY l.issueDate DESC, l.loanID DESC", (member_id, days, days))
    total = read_one("SELECT COUNT(*) AS total, COALESCE(SUM(returnDate IS NULL),0) AS stillOut "
                     "FROM LOANS l WHERE l.memberID = %s " + window, (member_id, days, days))
    return jsonify({"member": member, "rows": rows,
                    "summary": {"total": int(total["total"]), "stillOut": int(total["stillOut"])}})


@api.get("/reports/most-borrowed")
def report_most_borrowed():
    days = int_arg("days", 0)
    limit = int_arg("limit", 10, 1, 50)
    rows = read(
        "SELECT b.bookID, b.title, b.author, COUNT(*) AS loans, "
        "COUNT(DISTINCT l.memberID) AS students "
        "FROM LOANS l JOIN BOOKS b ON b.bookID = l.bookID "
        "WHERE %s <= 0 OR l.issueDate >= DATE_SUB(CURDATE(), INTERVAL %s DAY) "
        "GROUP BY b.bookID, b.title, b.author "
        "ORDER BY COUNT(*) DESC, b.title, b.bookID LIMIT %s", (days, days, limit))
    return jsonify({"rows": rows})


@api.get("/reports/fines-by-member")
def report_fines_by_member():
    rows = read(
        "SELECT m.memberID, m.name, COUNT(*) AS fines, SUM(f.amount) AS owed, "
        "MIN(f.createdAt) AS oldest FROM FINES f "
        "JOIN LOANS l ON l.loanID = f.loanID JOIN MEMBERS m ON m.memberID = l.memberID "
        "WHERE f.status = 'UNPAID' GROUP BY m.memberID, m.name "
        "ORDER BY SUM(f.amount) DESC, m.memberID")
    t = read_one(
        "SELECT COALESCE(SUM(CASE WHEN status='UNPAID' THEN amount END),0) AS owed, "
        "COALESCE(SUM(CASE WHEN status='PAID' THEN amount END),0) AS collected, "
        "COALESCE(SUM(status='UNPAID'),0) AS owedCount, COALESCE(SUM(status='PAID'),0) AS paidCount "
        "FROM FINES")
    return jsonify({"rows": rows, "totals": {
        "owed": t["owed"], "collected": t["collected"],
        "owedCount": int(t["owedCount"]), "paidCount": int(t["paidCount"])}})


@api.get("/reports/activity")
def report_activity():
    months = int_arg("months", 0)
    back = months - 1 if months > 0 else 0
    rows = read(
        "SELECT DATE_FORMAT(issueDate, '%%Y-%%m') AS month, COUNT(*) AS issued, "
        "COALESCE(SUM(returnDate IS NULL),0) AS stillOut, "
        "COALESCE(SUM(returnDate > dueDate),0) AS returnedLate FROM LOANS "
        "WHERE %s <= 0 OR issueDate >= DATE_SUB(DATE_SUB(CURDATE(), "
        "INTERVAL DAYOFMONTH(CURDATE()) - 1 DAY), INTERVAL %s MONTH) "
        "GROUP BY DATE_FORMAT(issueDate, '%%Y-%%m') "
        "ORDER BY DATE_FORMAT(issueDate, '%%Y-%%m') DESC", (months, back))
    return jsonify({"rows": rows})


@api.get("/reports/reservation-status")
def report_reservation_status():
    by_status = read("SELECT status, COUNT(*) AS count FROM RESERVATIONS GROUP BY status "
                     "ORDER BY FIELD(status,'WAITING','READY','FULFILLED','CANCELLED','EXPIRED')")
    titles = read(
        "SELECT b.bookID, b.title, b.availableCopies AS free, "
        "SUM(r.status = 'WAITING') AS waiting, SUM(r.status = 'READY') AS ready, "
        "MIN(CASE WHEN r.status = 'WAITING' THEN r.reservationDate END) AS oldestWaiting "
        "FROM RESERVATIONS r JOIN BOOKS b ON b.bookID = r.bookID "
        "WHERE r.status IN ('WAITING','READY') GROUP BY b.bookID, b.title, b.availableCopies "
        "ORDER BY SUM(r.status = 'WAITING') DESC, b.bookID")
    return jsonify({"byStatus": by_status, "titles": titles})
