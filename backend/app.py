
from flask import Flask, request, jsonify, send_from_directory
import mysql.connector
from mysql.connector import Error
from pathlib import Path

app = Flask(__name__)

# Locate the frontend folder beside the backend folder
BASE_DIR = Path(__file__).resolve().parent.parent
FRONTEND_DIR = BASE_DIR / "frontend"


def get_db_connection():
    return mysql.connector.connect(
        host="localhost",
        user="root",
        password="Pikachu06@",
        database="library_db"
    )


# Open the website
@app.route("/")
def home():
    return send_from_directory(FRONTEND_DIR, "index.html")


# Serve CSS and JavaScript
@app.route("/<path:filename>")
def frontend_files(filename):
    if filename in ("style.css", "script.js"):
        return send_from_directory(FRONTEND_DIR, filename)

    return jsonify({"error": "File not found"}), 404


# Fetch all books from MySQL
@app.route("/api/books", methods=["GET"])
def get_books():
    connection = None
    cursor = None

    try:
        connection = get_db_connection()
        cursor = connection.cursor(dictionary=True)

        cursor.execute("""
            SELECT bookID, title, author, category,
                   totalCopies, availableCopies
            FROM BOOKS
        """)

        books = cursor.fetchall()
        return jsonify(books)

    except Error as error:
        return jsonify({"error": str(error)}), 500

    finally:
        if cursor:
            cursor.close()
        if connection and connection.is_connected():
            connection.close()
            
# Add a book to MySQL
@app.route("/api/books", methods=["POST"])
def add_book():
    data = request.get_json(silent=True) or {}

    title = data.get("title", "").strip()
    author = data.get("author", "").strip()
    category = data.get("category", "").strip()

    if not title or not author or not category:
        return jsonify({
            "error": "Title, author and category are required."
        }), 400

    connection = None
    cursor = None

    try:
        connection = get_db_connection()
        cursor = connection.cursor()

        # Generate the next book ID
        cursor.execute(
            "SELECT COALESCE(MAX(bookID), 0) + 1 FROM BOOKS"
        )
        book_id = cursor.fetchone()[0]

        cursor.execute("""
            INSERT INTO BOOKS
                (bookID, title, author, category,
                 totalCopies, availableCopies)
            VALUES (%s, %s, %s, %s, %s, %s)
        """, (book_id, title, author, category, 1, 1))

        connection.commit()

        return jsonify({
            "message": "Book added successfully!",
            "bookID": book_id,
            "title": title,
            "author": author,
            "category": category
        }), 201

    except Error as error:
        if connection:
            connection.rollback()
        return jsonify({"error": str(error)}), 500

    finally:
        if cursor:
            cursor.close()
        if connection and connection.is_connected():
            connection.close()

# Fetch all members from MySQL
@app.route("/api/members", methods=["GET"])
def get_members():
    connection = None
    cursor = None

    try:
        connection = get_db_connection()
        cursor = connection.cursor(dictionary=True)

        cursor.execute("""
            SELECT memberID, registrationNo, name, email,
                   phone, memberType, active
            FROM MEMBERS
            ORDER BY memberID
        """)

        members = cursor.fetchall()
        return jsonify(members)

    except Error as error:
        return jsonify({"error": str(error)}), 500

    finally:
        if cursor:
            cursor.close()
        if connection and connection.is_connected():
            connection.close()


# Register a new member in MySQL
@app.route("/api/members", methods=["POST"])
def add_member():
    data = request.get_json(silent=True) or {}

    registration_no = str(data.get("registrationNo") or "").strip()
    name = str(data.get("name") or "").strip()
    email = str(data.get("email") or "").strip() or None
    phone = str(data.get("phone") or "").strip() or None
    member_type = str(data.get("memberType") or "").strip()

    if not registration_no or not name or not member_type:
        return jsonify({
            "error": "Registration number, name and member type are required."
        }), 400

    connection = None
    cursor = None

    try:
        connection = get_db_connection()
        cursor = connection.cursor()

        # Generate the next member ID
        cursor.execute(
            "SELECT COALESCE(MAX(memberID), 0) + 1 FROM MEMBERS"
        )
        member_id = cursor.fetchone()[0]

        cursor.execute("""
            INSERT INTO MEMBERS
                (memberID, registrationNo, name, email, phone,
                 memberType, active)
            VALUES (%s, %s, %s, %s, %s, %s, %s)
        """, (
            member_id,
            registration_no,
            name,
            email,
            phone,
            member_type,
            1
        ))

        connection.commit()

        return jsonify({
            "message": "Member registered successfully!",
            "memberID": member_id
        }), 201

    except Error as error:
        if connection:
            connection.rollback()

        # Handles duplicate registration numbers or emails
        if getattr(error, "errno", None) == 1062:
            return jsonify({
                "error": "This registration number or email already exists."
            }), 409

        return jsonify({"error": str(error)}), 500

    finally:
        if cursor:
            cursor.close()
        if connection and connection.is_connected():
            connection.close()
if __name__ == "__main__":
    app.run(host="127.0.0.1", port=5000, debug=True)