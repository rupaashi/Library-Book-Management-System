import { api, getConfig } from "../api.js";
import { h, pageHeader, dataTable, statusBadge, makeField, toast, fmtDate, orDash, confirmDialog, loadingView, errorView } from "../ui.js";

export async function reservationsPage(root, isCurrent) {
    const cfg = await getConfig().catch(() => null);
    root.append(pageHeader({
        title: "Reservations",
        description: `Students queue for a title when no copy is free. The oldest waiting reservation is served first${cfg ? `, and a ready copy is held for ${cfg.holdDays} days` : ""}.`,
    }));

    /* ---------------------------------------------------------- reserve form */
    let students = [], books = [];
    const studentField = makeField({ name: "memberID", label: "Student", type: "lookup", required: true, items: [], hint: "Type a name, registration number or ID." });
    const bookField = makeField({ name: "bookID", label: "Book", type: "lookup", required: true, items: [], hint: "Type a title, author or ID." });
    const availability = h("p", { class: "hint", role: "status" });
    const formError = h("div", { class: "alert alert-error", role: "alert", hidden: true });
    const reserveBtn = h("button", { class: "btn btn-primary", type: "submit" }, "Place reservation");

    bookField.input.addEventListener("input", () => {
        const b = books.find((x) => x.bookID === bookField.getValue());
        availability.textContent = !b ? "" : b.availableCopies > 0
            ? `${b.availableCopies} copy(ies) on the shelf. If one is not held for another student, borrow it instead of reserving.`
            : "No copies are free, so a reservation is the right choice.";
    });

    const form = h("form", { class: "form-panel", novalidate: true, onsubmit: async (e) => {
        e.preventDefault();
        formError.hidden = true;
        const bad = [studentField, bookField].filter((f) => !f.validate());
        if (bad.length) { bad[0].input.focus(); return; }
        reserveBtn.disabled = true; reserveBtn.textContent = "Reserving\u2026";
        try {
            toast((await api.post("/api/reservations", { memberID: studentField.getValue(), bookID: bookField.getValue() })).message);
            bookField.input.value = ""; availability.textContent = "";
            if (isCurrent()) load();
        } catch (err) {
            formError.textContent = err.message; formError.hidden = false;
        } finally {
            reserveBtn.disabled = false; reserveBtn.textContent = "Place reservation";
        }
    } },
        formError,
        h("div", { class: "form-grid" }, studentField.el, h("div", {}, bookField.el, availability)),
        h("div", { class: "mt" }, reserveBtn));

    root.append(h("section", { class: "section", "aria-labelledby": "resH" }, h("h2", { id: "resH" }, "Reserve a book"), form));

    try {
        [students, books] = await Promise.all([api.get("/api/members"), api.get("/api/books")]);
        studentField.setItems(students.filter((m) => m.active && m.memberType === "Student")
            .map((m) => ({ id: m.memberID, label: `${m.memberID} \u00B7 ${m.name} (${m.registrationNo})` })));
        bookField.setItems(books.filter((b) => b.active).map((b) => ({ id: b.bookID, label: `${b.bookID} \u00B7 ${b.title} \u2014 ${b.author}` })));
    } catch (err) {
        formError.textContent = `Could not load students and books: ${err.message}`; formError.hidden = false;
    }

    /* ---------------------------------------------------------- table */
    const filters = { status: "live", q: "" };
    const status = h("select", { class: "input", id: "rs-s", onchange: (e) => { filters.status = e.target.value; load(); } },
        h("option", { value: "live" }, "Waiting and ready"), h("option", { value: "all" }, "All reservations"));
    const search = h("input", { class: "input", id: "rs-q", type: "search", placeholder: "Student, book or reservation ID", oninput: (e) => { filters.q = e.target.value; draw(); } });
    const results = h("div");
    let rows = [];

    root.append(h("section", { class: "section", "aria-labelledby": "rlH" },
        h("h2", { id: "rlH" }, "Reservation queue"),
        h("div", { class: "toolbar mt" },
            h("div", { class: "field" }, h("label", { for: "rs-s" }, "Show"), status),
            h("div", { class: "field grow" }, h("label", { for: "rs-q" }, "Search"), search)),
        results));

    async function load() {
        results.replaceChildren(loadingView());
        try {
            // Release holds that were not collected in time, so the list is current.
            const swept = await api.post("/api/reservations/expire").catch(() => null);
            if (swept && swept.expired > 0) toast(swept.message, "info");
            rows = await api.get(`/api/reservations?status=${filters.status}`);
            if (isCurrent()) draw(true);
        } catch (err) { results.replaceChildren(errorView(err.message, load)); }
    }

    function draw(force) {
        if (!force && results.querySelector(".state.error")) return;
        const q = filters.q.toLowerCase().trim();
        const shown = rows.filter((r) => !q || [r.reservationID, r.title, r.memberName].some((v) => String(v ?? "").toLowerCase().includes(q)));
        results.replaceChildren(h("p", { class: "count", role: "status" }, `${shown.length} reservation(s)`), dataTable({
            caption: "Reservations", rows: shown, emptyTitle: "No reservations", emptyText: "Reservations appear here when a student queues for a book.",
            defaultSort: { key: "reservationDate", dir: "asc" },
            columns: [
                { key: "reservationID", label: "ID", align: "right" },
                { key: "title", label: "Book" },
                { key: "memberName", label: "Student" },
                { key: "reservationDate", label: "Reserved", render: (r) => fmtDate(r.reservationDate) },
                { label: "Status", sortValue: (r) => r.status, render: (r) => statusBadge(r.status) },
                { key: "queuePosition", label: "Queue position", align: "right", render: (r) => r.status === "WAITING" ? `#${r.queuePosition}` : r.status === "READY" ? "Ready to collect" : orDash("") },
                { key: "expiryDate", label: "Held until", render: (r) => fmtDate(r.expiryDate) },
                { label: "Actions", sortable: false, render: (r) => ["WAITING", "READY"].includes(r.status)
                    ? h("button", { class: "btn btn-danger-outline btn-sm", type: "button", "aria-label": `Cancel reservation ${r.reservationID}`, onclick: () => cancel(r) }, "Cancel") : orDash("") },
            ],
        }));
    }

    async function cancel(r) {
        const ok = await confirmDialog({
            title: "Cancel this reservation?", danger: true, confirmLabel: "Cancel reservation",
            message: `${r.memberName}'s reservation for \u201C${r.title}\u201D will be cancelled${r.status === "READY" ? ", and the held copy passes to the next student in the queue" : ""}.`,
        });
        if (!ok) return;
        try {
            const res = await api.post(`/api/reservations/${r.reservationID}/cancel`);
            toast(res.message);
            for (const n of res.readyReservations || [])
                toast(`Reservation ${n.reservationID} is now ready: ${n.memberName} can borrow \u201C${n.title}\u201D until ${fmtDate(n.expiryDate)}.`, "info");
            if (isCurrent()) load();
        } catch (err) { toast(err.message, "error"); }
    }

    await load();
}
