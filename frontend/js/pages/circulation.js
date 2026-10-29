import { api, getConfig } from "../api.js";
import { h, pageHeader, mountAsync, dataTable, statusBadge, makeField, toast, fmtDate, money, orDash, loadingView, errorView } from "../ui.js";

export async function circulationPage(root, isCurrent) {
    const cfg = await getConfig().catch(() => null);
    root.append(pageHeader({
        title: "Issue and Return",
        description: cfg
            ? `Loans run for ${cfg.loanPeriodDays} days and can be renewed up to ${cfg.maxRenewals} times. Fine: \u20B9${cfg.finePerDay} per overdue day.`
            : "Issue books, take returns and renew loans.",
    }));

    /* ---------------------------------------------------------- issue form */
    let students = [], books = [];
    const studentItems = () => students.filter((m) => m.active && m.memberType === "Student")
        .map((m) => ({ id: m.memberID, label: `${m.memberID} \u00B7 ${m.name} (${m.registrationNo})` }));
    const bookItems = () => books.filter((b) => b.active)
        .map((b) => ({ id: b.bookID, label: `${b.bookID} \u00B7 ${b.title} \u2014 ${b.author}` }));

    const studentField = makeField({ name: "memberID", label: "Student", type: "lookup", required: true, items: [], hint: "Type a name, registration number or ID." });
    const bookField = makeField({ name: "bookID", label: "Book", type: "lookup", required: true, items: [], hint: "Type a title, author or ID." });
    const availability = h("p", { class: "hint", role: "status" });
    const formError = h("div", { class: "alert alert-error", role: "alert", hidden: true });
    const issueBtn = h("button", { class: "btn btn-primary", type: "submit" }, "Issue book");

    function updateAvailability() {
        const id = bookField.getValue();
        const b = books.find((x) => x.bookID === id);
        if (!b) { availability.textContent = ""; issueBtn.disabled = false; return; }
        if (b.availableCopies > 0) {
            availability.textContent = `${b.availableCopies} of ${b.totalCopies} copies available.`;
            issueBtn.disabled = false;
        } else {
            availability.textContent = "No copies are free right now. Place a reservation instead.";
            issueBtn.disabled = true;
        }
    }
    bookField.input.addEventListener("input", updateAvailability);

    const form = h("form", { class: "form-panel", novalidate: true, onsubmit: async (e) => {
        e.preventDefault();
        formError.hidden = true;
        const bad = [studentField, bookField].filter((f) => !f.validate());
        if (bad.length) { bad[0].input.focus(); return; }
        issueBtn.disabled = true; issueBtn.textContent = "Issuing\u2026";
        try {
            const res = await api.post("/api/loans", { memberID: studentField.getValue(), bookID: bookField.getValue() });
            toast(res.message);
            bookField.input.value = ""; availability.textContent = "";
            if (isCurrent()) { await loadFormData(); loadLoans(); }
        } catch (err) {
            formError.textContent = err.message; formError.hidden = false;
        } finally {
            issueBtn.textContent = "Issue book"; updateAvailability();
        }
    } },
        formError,
        h("div", { class: "form-grid" }, studentField.el, h("div", {}, bookField.el, availability)),
        h("div", { class: "mt" }, issueBtn));

    root.append(h("section", { class: "section", "aria-labelledby": "issueH" },
        h("h2", { id: "issueH" }, "Issue a book"), form));

    async function loadFormData() {
        [students, books] = await Promise.all([api.get("/api/members"), api.get("/api/books")]);
        studentField.setItems(studentItems());
        bookField.setItems(bookItems());
    }
    try { await loadFormData(); }
    catch (err) { formError.textContent = `Could not load students and books: ${err.message}`; formError.hidden = false; }

    /* ---------------------------------------------------------- loans table */
    const filters = { status: "active", q: "" };
    const status = h("select", { class: "input", id: "ln-s", onchange: (e) => { filters.status = e.target.value; loadLoans(); } },
        h("option", { value: "active" }, "Not yet returned"), h("option", { value: "overdue" }, "Overdue only"),
        h("option", { value: "returned" }, "Returned"), h("option", { value: "all" }, "All loans"));
    const search = h("input", { class: "input", id: "ln-q", type: "search", placeholder: "Student, book or loan ID", oninput: (e) => { filters.q = e.target.value; draw(); } });
    const results = h("div");
    let loans = [];

    root.append(h("section", { class: "section", "aria-labelledby": "loansH" },
        h("h2", { id: "loansH" }, "Loans"),
        h("div", { class: "toolbar mt" },
            h("div", { class: "field" }, h("label", { for: "ln-s" }, "Show"), status),
            h("div", { class: "field grow" }, h("label", { for: "ln-q" }, "Search"), search)),
        results));

    async function loadLoans() {
        results.replaceChildren(loadingView());
        try { loans = await api.get(`/api/loans?status=${filters.status}`); if (isCurrent()) draw(true); }
        catch (err) { results.replaceChildren(errorView(err.message, loadLoans)); }
    }

    function draw(force) {
        if (!force && results.querySelector(".state.error")) return;
        const q = filters.q.toLowerCase().trim();
        const rows = loans.filter((l) => !q || [l.loanID, l.title, l.memberName, l.registrationNo].some((v) => String(v ?? "").toLowerCase().includes(q)));
        results.replaceChildren(h("p", { class: "count", role: "status" }, `${rows.length} loan(s)`), dataTable({
            caption: "Loans", rows, emptyTitle: "No loans found", emptyText: "Change the filter or issue a book above.",
            columns: [
                { key: "loanID", label: "Loan", align: "right" },
                { key: "title", label: "Book" },
                { key: "memberName", label: "Student", render: (l) => h("span", {}, l.memberName, h("br"), h("span", { class: "hint" }, l.registrationNo)) },
                { key: "issueDate", label: "Issued", render: (l) => fmtDate(l.issueDate) },
                { key: "dueDate", label: "Due", render: (l) => fmtDate(l.dueDate) },
                { key: "returnDate", label: "Returned", render: (l) => fmtDate(l.returnDate) },
                { key: "renewalCount", label: "Renewals", align: "right", render: (l) => cfg ? `${l.renewalCount} / ${cfg.maxRenewals}` : l.renewalCount },
                { label: "Status", sortValue: (l) => l.state, render: (l) => h("div", {}, statusBadge(l.state),
                    l.state === "OVERDUE" ? h("div", { class: "hint" }, `${l.daysOverdue} day(s) late; fine if returned today ${money(l.fineIfReturnedToday)}`) : null,
                    l.fineAmount != null ? h("div", { class: "hint" }, `Fine ${money(l.fineAmount)} (${l.fineStatus.toLowerCase()})`) : null) },
                { label: "Actions", sortable: false, render: (l) => l.state === "RETURNED" ? orDash("") : actions(l) },
            ],
        }));
    }

    function actions(l) {
        const atLimit = cfg && l.renewalCount >= cfg.maxRenewals;
        const blocked = l.state === "OVERDUE" ? "Overdue loans cannot be renewed. Return the book." : atLimit ? "The renewal limit has been reached." : "";
        const ret = h("button", { class: "btn btn-primary btn-sm", type: "button", "aria-label": `Return loan ${l.loanID}`, onclick: () => run(ret, `/api/loans/${l.loanID}/return`, true) }, "Return");
        const ren = h("button", { class: "btn btn-secondary btn-sm", type: "button", "aria-label": `Renew loan ${l.loanID}`, title: blocked || null, disabled: !!blocked, onclick: () => run(ren, `/api/loans/${l.loanID}/renew`, false) }, "Renew");
        return h("div", { class: "actions" }, ret, ren);
    }

    async function run(button, path, isReturn) {
        button.disabled = true;
        try {
            const res = await api.post(path);
            toast(res.message, res.fineAmount ? "info" : "success");
            for (const r of res.readyReservations || [])
                toast(`Reservation ${r.reservationID} is now ready: ${r.memberName} can borrow \u201C${r.title}\u201D until ${fmtDate(r.expiryDate)}.`, "info");
            if (isReturn) await loadFormData().catch(() => {});
            if (isCurrent()) loadLoans();
        } catch (err) {
            toast(err.message, "error");
            button.disabled = false;
        }
    }

    await loadLoans();
}
