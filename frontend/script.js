
let books = [];

const bookTable = document.getElementById("bookTable");
const searchInput = document.getElementById("searchInput");
const categoryFilter = document.getElementById("categoryFilter");
const addBookBtn = document.getElementById("addBookBtn");
const totalBooks = document.getElementById("totalBooks");

// Load books from the Flask backend
async function loadBooks() {
    try {
        const response = await fetch("/api/books");
        const data = await response.json();

        if (!response.ok) {
            throw new Error(data.error || "Could not load books");
        }

        books = data;
        displayBooks();

        totalBooks.textContent = books.length.toLocaleString();

    } catch (error) {
        console.error("Error loading books:", error);

        bookTable.innerHTML =
            '<tr><td colspan="5">Could not load books. Check the backend.</td></tr>';
    }
}

// Display books in the table
function displayBooks() {
    const searchText = searchInput.value.toLowerCase().trim();
    const selectedCategory = categoryFilter.value;

    const filteredBooks = books.filter(book => {
        const title = String(book.title || "").toLowerCase();
        const author = String(book.author || "").toLowerCase();
        const id = String(book.bookID ?? "").toLowerCase();
        const category = String(book.category || "");

        const matchesSearch =
            title.includes(searchText) ||
            author.includes(searchText) ||
            id.includes(searchText);

        const matchesCategory =
            selectedCategory === "All" ||
            category === selectedCategory;

        return matchesSearch && matchesCategory;
    });

    bookTable.replaceChildren();

    if (filteredBooks.length === 0) {
        bookTable.innerHTML =
            '<tr><td colspan="5">No books found.</td></tr>';
        return;
    }

    filteredBooks.forEach(book => {
        const row = document.createElement("tr");

        const statusText =
            Number(book.availableCopies ?? 0) > 0
                ? "Available"
                : "Unavailable";

        const values = [
            book.bookID,
            book.title,
            book.author,
            book.category,
            statusText
        ];

        values.forEach((value, index) => {
            const cell = document.createElement("td");

            if (index === 4) {
                const badge = document.createElement("span");

                badge.className = "status";
                badge.textContent = value;

                if (value === "Unavailable") {
                    badge.classList.add("unavailable");
                }

                cell.appendChild(badge);
            } else {
                cell.textContent = value ?? "";
            }

            row.appendChild(cell);
        });

        bookTable.appendChild(row);
    });
}

// Search books
searchInput.addEventListener("input", displayBooks);

// Filter books by category
categoryFilter.addEventListener("change", displayBooks);

// Add a new book
addBookBtn.onclick = async function () {
    const title = prompt("Enter book title:");

    if (!title || !title.trim()) {
        return;
    }

    const author = prompt("Enter author's name:");

    if (!author || !author.trim()) {
        return;
    }

    const category = prompt(
        "Enter category: Technology, Science, Fiction, or Management"
    );

    if (!category || !category.trim()) {
        return;
    }

    const categories = [
        "Technology",
        "Science",
        "Fiction",
        "Management"
    ];

    const matchedCategory = categories.find(
        item => item.toLowerCase() === category.trim().toLowerCase()
    );

    if (!matchedCategory) {
        alert(
            "Invalid category. Please enter Technology, Science, Fiction, or Management."
        );
        return;
    }

    try {
        const response = await fetch("/api/books", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({
                title: title.trim(),
                author: author.trim(),
                category: matchedCategory
            })
        });

        const result = await response.json();

        if (!response.ok) {
            throw new Error(result.error || "Could not add book");
        }

        alert("Book saved to MySQL successfully!");

        // Reload the table to show the new book
        await loadBooks();

    } catch (error) {
        console.error("Error adding book:", error);
        alert("Could not save book: " + error.message);
    }
};

// Load books when the page opens
loadBooks();

const memberTable = document.getElementById("memberTable");
const memberSearchInput = document.getElementById("memberSearchInput");
const addMemberBtn = document.getElementById("addMemberBtn");
const totalMembers = document.getElementById("totalMembers");

let members = [];

// Fetch members from MySQL through Flask
async function loadMembers() {
    try {
        const response = await fetch("/api/members");
        const data = await response.json();

        if (!response.ok) {
            throw new Error(data.error || "Could not load members");
        }

        members = data;

        totalMembers.textContent = members.length.toLocaleString();

        displayMembers();

    } catch (error) {
        console.error("Error loading members:", error);

        memberTable.innerHTML =
            '<tr><td colspan="7">Could not load members. Check the backend.</td></tr>';
    }
}

// Display members in the table
function displayMembers() {
    const searchText = memberSearchInput.value.toLowerCase().trim();

    const filteredMembers = members.filter(member => {
        const name = String(member.name || "").toLowerCase();
        const registrationNo =
            String(member.registrationNo || "").toLowerCase();

        return name.includes(searchText) ||
               registrationNo.includes(searchText);
    });

    memberTable.replaceChildren();

    if (filteredMembers.length === 0) {
        memberTable.innerHTML =
            '<tr><td colspan="7">No members found.</td></tr>';
        return;
    }

    filteredMembers.forEach(member => {
        const row = document.createElement("tr");

        const values = [
            member.memberID,
            member.registrationNo,
            member.name,
            member.email || "-",
            member.phone || "-",
            member.memberType,
            Number(member.active) === 1 ? "Active" : "Inactive"
        ];

        values.forEach(value => {
            const cell = document.createElement("td");
            cell.textContent = value ?? "";
            row.appendChild(cell);
        });

        memberTable.appendChild(row);
    });
}

// Search members
memberSearchInput.addEventListener("input", displayMembers);

// Register a new member
addMemberBtn.onclick = async function () {
    const registrationNo = prompt("Enter registration number:");
    if (!registrationNo || !registrationNo.trim()) return;

    const name = prompt("Enter member name:");
    if (!name || !name.trim()) return;

    const emailInput = prompt("Enter email address (optional):");
    if (emailInput === null) return;

    const phoneInput = prompt("Enter phone number (optional):");
    if (phoneInput === null) return;

    const memberType = prompt(
        "Enter member type (Student, Faculty, or Staff):"
    );

    if (!memberType || !memberType.trim()) return;

    try {
        const response = await fetch("/api/members", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({
                registrationNo: registrationNo.trim(),
                name: name.trim(),
                email: emailInput.trim(),
                phone: phoneInput.trim(),
                memberType: memberType.trim()
            })
        });

        const result = await response.json();

        if (!response.ok) {
            throw new Error(result.error || "Could not register member");
        }

        alert("Member registered successfully!");

        await loadMembers();

    } catch (error) {
        console.error("Error registering member:", error);
        alert("Could not register member: " + error.message);
    }
};

// Load members when the page opens
loadMembers();