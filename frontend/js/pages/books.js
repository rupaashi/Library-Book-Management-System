import { api } from "../api.js";
import { h, pageHeader, mountAsync, dataTable, statusBadge, formDialog, confirmDialog, infoDialog, toast, orDash } from "../ui.js";

export async function booksPage(root, isCurrent) {
    let books = [];
    const filters = { q: "", category: "", availability: "", inactive: false };

    const addBtn = h("button", { class: "btn btn-primary", type: "button", onclick: addBook }, "Add book");
    root.append(pageHeader({ title: "Books", description: "Search the catalogue and manage titles and copies.", actions: addBtn }));

    const search = h("input", { class: "input", id: "bk-q", type: "search", placeholder: "Title, author, ISBN or ID", oninput: (e) => { filters.q = e.target.value; draw(); } });
    const category = h("select", { class: "input", id: "bk-cat", onchange: (e) => { filters.category = e.target.value; draw(); } });
    const avail = h("select", { class: "input", id: "bk-av", onchange: (e) => { filters.availability = e.target.value; draw(); } },
        h("option", { value: "" }, "All"), h("option", { value: "yes" }, "Available now"), h("option", { value: "no" }, "No copies free"));
    const inactive = h("input", { type: "checkbox", id: "bk-in", onchange: (e) => { filters.inactive = e.target.checked; draw(); } });

    root.append(h("div", { class: "toolbar" },
        h("div", { class: "field grow" }, h("label", { for: "bk-q" }, "Search"), search),
        h("div", { class: "field" }, h("label", { for: "bk-cat" }, "Category"), category),
        h("div", { class: "field" }, h("label", { for: "bk-av" }, "Availability"), avail),
        h("label", { class: "check", for: "bk-in" }, inactive, "Show inactive")));

    const results = h("div");
    root.append(results);

    async function load() {
        await mountAsync(results, () => api.get("/api/books"), (data) => {
            books = data;
            const cats = [...new Set(books.map((b) => b.category).filter(Boolean))].sort();
            category.replaceChildren(h("option", { value: "" }, "All categories"), ...cats.map((c) => h("option", { value: c }, c)));
            category.value = cats.includes(filters.category) ? filters.category : "";
            return buildTable();
        });
    }

    function filtered() {
        const q = filters.q.toLowerCase().trim();
        return books.filter((b) => {
            if (!filters.inactive && !b.active) return false;
            if (filters.category && b.category !== filters.category) return false;
            if (filters.availability === "yes" && !(b.availableCopies > 0)) return false;
            if (filters.availability === "no" && b.availableCopies > 0) return false;
            if (!q) return true;
            return [b.bookID, b.title, b.author, b.isbn].some((v) => String(v ?? "").toLowerCase().includes(q));
        });
    }

    function buildTable() {
        const rows = filtered();
        return h("div", {},
            h("p", { class: "count", role: "status" }, `${rows.length} of ${books.length} book(s)`),
            dataTable({
                caption: "Book catalogue", rows, emptyTitle: "No books match your search",
                emptyText: "Try a different search or clear the filters.",
                columns: [
                    { key: "bookID", label: "ID", align: "right" },
                    { key: "title", label: "Title" },
                    { key: "author", label: "Author" },
                    { key: "category", label: "Category" },
                    { key: "isbn", label: "ISBN", muted: true, render: (b) => orDash(b.isbn) },
                    { key: "availableCopies", label: "Available / total", align: "right", render: (b) => `${b.availableCopies} / ${b.totalCopies}` },
                    { label: "Status", sortValue: (b) => (!b.active ? "Inactive" : b.availableCopies > 0 ? "Available" : "Unavailable"),
                        render: (b) => statusBadge(!b.active ? "INACTIVE" : b.availableCopies > 0 ? "AVAILABLE" : "UNAVAILABLE") },
                    { label: "Actions", sortable: false, render: (b) => h("div", { class: "actions" },
                        h("button", { class: "btn btn-secondary btn-sm", type: "button", "aria-label": `View ${b.title}`, onclick: () => view(b) }, "View"),
                        h("button", { class: "btn btn-secondary btn-sm", type: "button", "aria-label": `Edit ${b.title}`, onclick: () => edit(b) }, "Edit"),
                        b.active ? h("button", { class: "btn btn-danger-outline btn-sm", type: "button", "aria-label": `Deactivate ${b.title}`, onclick: () => deactivate(b) }, "Deactivate") : null) },
                ],
            }));
    }

    function draw() {
        if (!books.length && results.querySelector(".state")) return;
        results.replaceChildren(buildTable());
    }

    const categoryItems = () => [...new Set(books.map((b) => b.category).filter(Boolean))];
    const bookFields = (withCopies) => [
        { name: "title", label: "Title", required: true, maxlength: 100 },
        { name: "author", label: "Author", required: true, maxlength: 100 },
        { name: "category", label: "Category", required: true, maxlength: 50, hint: categoryItems().length ? `Existing: ${categoryItems().join(", ")}` : null },
        { name: "isbn", label: "ISBN", maxlength: 20, hint: "Optional. Must be unique." },
        { name: "publisher", label: "Publisher", maxlength: 100 },
        ...(withCopies ? [{ name: "totalCopies", label: "Number of copies", type: "number", required: true, min: 1, max: 1000, value: 1 }] : []),
    ];

    async function addBook() {
        const saved = await formDialog({
            title: "Add book", submitLabel: "Add book", fields: bookFields(true),
            onSubmit: async (v) => {
                const res = await api.post("/api/books", { ...v, totalCopies: Number(v.totalCopies) });
                toast(`${res.message} (ID ${res.bookID})`);
            },
        });
        if (saved && isCurrent()) load();
    }

    async function edit(b) {
        const saved = await formDialog({
            title: `Edit book ${b.bookID}`, intro: "Copy counts change through issuing and returning, so they are not edited here.",
            fields: bookFields(false), initial: b,
            onSubmit: async (v) => { const res = await api.patch(`/api/books/${b.bookID}`, v); toast(res.message); },
        });
        if (saved && isCurrent()) load();
    }

    function view(b) {
        infoDialog({ title: b.title, rows: [
            ["Book ID", b.bookID], ["Author", b.author], ["Category", orDash(b.category)], ["ISBN", orDash(b.isbn)],
            ["Publisher", orDash(b.publisher)], ["Copies", `${b.availableCopies} available of ${b.totalCopies}`],
            ["Status", b.active ? "Active" : "Inactive"],
        ] });
    }

    async function deactivate(b) {
        const ok = await confirmDialog({
            title: "Deactivate this book?", danger: true, confirmLabel: "Deactivate",
            message: `\u201C${b.title}\u201D will no longer be issued or reserved. Its loan history is kept.`,
        });
        if (!ok) return;
        try { toast((await api.post(`/api/books/${b.bookID}/deactivate`)).message); load(); }
        catch (err) { toast(err.message, "error"); }
    }

    await load();
}
