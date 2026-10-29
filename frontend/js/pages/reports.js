import { api } from "../api.js";
import { h, pageHeader, dataTable, statusBadge, makeField, downloadCsv, fmtDate, money, orDash, loadingView, errorView, emptyView } from "../ui.js";

/** A titled table with its own CSV export button. */
function reportTable({ heading, filename, columns, rows, emptyTitle }) {
    const exportable = columns.filter((c) => c.label !== "Actions");
    return h("div", { class: "mt" },
        h("div", { class: "page-head", style: "margin-bottom:8px" },
            h("h2", { style: "font-size:1.0625rem" }, heading),
            rows.length ? h("button", { class: "btn btn-secondary btn-sm", type: "button",
                onclick: () => downloadCsv(filename, exportable, rows) }, "Export CSV") : null),
        dataTable({ caption: heading, rows, columns, emptyTitle: emptyTitle || "No data for this report" }));
}

const date = (key) => (r) => fmtDate(r[key]);

const REPORTS = [
    {
        id: "overdue", label: "Overdue loans", params: [],
        description: "Every loan past its due date and not yet returned, oldest first.",
        async run() {
            const d = await api.get("/api/reports/overdue");
            return [reportTable({ heading: "Overdue loans", filename: "overdue-loans.csv", rows: d.rows, emptyTitle: "No overdue loans", columns: [
                { key: "loanID", label: "Loan", align: "right" }, { key: "memberName", label: "Student" }, { key: "registrationNo", label: "Registration no." },
                { key: "title", label: "Book" }, { key: "issueDate", label: "Issued", render: date("issueDate"), csv: (r) => r.issueDate },
                { key: "dueDate", label: "Due", render: date("dueDate"), csv: (r) => r.dueDate },
                { key: "daysOverdue", label: "Days overdue", align: "right" },
                { key: "fineIfReturnedToday", label: "Fine if returned today", align: "right", render: (r) => money(r.fineIfReturnedToday), csv: (r) => r.fineIfReturnedToday },
            ] })];
        },
    },
    {
        id: "history", label: "Student borrowing history",
        description: "Every loan of one student with its state and any fine.",
        params: [
            { name: "memberID", label: "Student", type: "lookup", required: true, needs: "students", hint: "Type a name, registration number or ID." },
            { name: "days", label: "Only loans from the last N days", type: "number", min: 0, max: 3650, value: 0, hint: "0 means all loans." },
        ],
        async run(v) {
            const d = await api.get(`/api/reports/history?memberID=${v.memberID}&days=${v.days || 0}`);
            return [
                reportTable({ heading: `Borrowing history: ${d.member.name} (${d.member.registrationNo})`, filename: `history-${v.memberID}.csv`, rows: d.rows, emptyTitle: "No loans found", columns: [
                    { key: "loanID", label: "Loan", align: "right" }, { key: "title", label: "Book" },
                    { key: "issueDate", label: "Issued", render: date("issueDate"), csv: (r) => r.issueDate },
                    { key: "dueDate", label: "Due", render: date("dueDate"), csv: (r) => r.dueDate },
                    { key: "returnDate", label: "Returned", render: date("returnDate"), csv: (r) => r.returnDate },
                    { key: "state", label: "State", render: (r) => statusBadge(r.state) },
                    { key: "fineAmount", label: "Fine", align: "right", render: (r) => r.fineAmount == null ? orDash("") : `${money(r.fineAmount)} (${r.fineStatus.toLowerCase()})`, csv: (r) => r.fineAmount },
                ] }),
                d.rows.length ? h("p", { class: "summary-line" }, `Total loans: ${d.summary.total} (${d.summary.stillOut} still out)`) : null,
            ];
        },
    },
    {
        id: "most-borrowed", label: "Most borrowed titles",
        description: "Titles ranked by number of loans, with how many different students borrowed them.",
        params: [
            { name: "days", label: "Only loans from the last N days", type: "number", min: 0, max: 3650, value: 0, hint: "0 means all time." },
            { name: "limit", label: "How many titles", type: "number", min: 1, max: 50, value: 10 },
        ],
        async run(v) {
            const d = await api.get(`/api/reports/most-borrowed?days=${v.days || 0}&limit=${v.limit || 10}`);
            return [reportTable({ heading: "Most borrowed titles", filename: "most-borrowed.csv", rows: d.rows, emptyTitle: "No loans recorded for this period", columns: [
                { key: "bookID", label: "ID", align: "right" }, { key: "title", label: "Title" }, { key: "author", label: "Author" },
                { key: "loans", label: "Loans", align: "right" }, { key: "students", label: "Different students", align: "right" },
            ] })];
        },
    },
    {
        id: "fines", label: "Outstanding fines by student", params: [],
        description: "Unpaid fines grouped per student, largest debt first, with library-wide totals.",
        async run() {
            const d = await api.get("/api/reports/fines-by-member");
            return [
                reportTable({ heading: "Outstanding fines by student", filename: "fines-by-student.csv", rows: d.rows, emptyTitle: "No unpaid fines", columns: [
                    { key: "memberID", label: "Student ID", align: "right" }, { key: "name", label: "Name" }, { key: "fines", label: "Fines", align: "right" },
                    { key: "owed", label: "Owes", align: "right", render: (r) => money(r.owed), csv: (r) => r.owed },
                    { key: "oldest", label: "Oldest fine", render: date("oldest"), csv: (r) => r.oldest },
                ] }),
                h("p", { class: "summary-line" }, `Still owed: ${money(d.totals.owed)} (${d.totals.owedCount} fine(s)). Collected so far: ${money(d.totals.collected)} (${d.totals.paidCount} fine(s)).`),
            ];
        },
    },
    {
        id: "activity", label: "Borrowing activity by month",
        description: "Loans issued per month, how many are still out and how many were returned late.",
        params: [{ name: "months", label: "Last N months", type: "number", min: 0, max: 120, value: 0, hint: "0 means all time; 1 means this month only." }],
        async run(v) {
            const d = await api.get(`/api/reports/activity?months=${v.months || 0}`);
            return [reportTable({ heading: "Borrowing activity by month", filename: "borrowing-activity.csv", rows: d.rows, emptyTitle: "No loans recorded for this period", columns: [
                { key: "month", label: "Month" }, { key: "issued", label: "Loans issued", align: "right" },
                { key: "stillOut", label: "Still out", align: "right" }, { key: "returnedLate", label: "Returned late", align: "right" },
            ] })];
        },
    },
    {
        id: "reservations", label: "Reservation status", params: [],
        description: "Reservations by status, then every title that has students waiting or a copy on hold.",
        async run() {
            const d = await api.get("/api/reports/reservation-status");
            if (!d.byStatus.length) return [emptyView("No reservations have been made yet")];
            return [
                reportTable({ heading: "Reservations by status", filename: "reservations-by-status.csv", rows: d.byStatus, columns: [
                    { key: "status", label: "Status", render: (r) => statusBadge(r.status), csv: (r) => r.status }, { key: "count", label: "Count", align: "right" },
                ] }),
                reportTable({ heading: "Titles with live reservations", filename: "titles-with-reservations.csv", rows: d.titles, emptyTitle: "No live reservations", columns: [
                    { key: "bookID", label: "ID", align: "right" }, { key: "title", label: "Title" }, { key: "free", label: "Free copies", align: "right" },
                    { key: "waiting", label: "Waiting", align: "right" }, { key: "ready", label: "Ready", align: "right" },
                    { key: "oldestWaiting", label: "Oldest waiting", render: date("oldestWaiting"), csv: (r) => r.oldestWaiting },
                ] }),
            ];
        },
    },
];

export async function reportsPage(root, isCurrent) {
    root.append(pageHeader({ title: "Reports", description: "Run a report on live data. Every table can be sorted and exported to CSV." }));

    let studentItems = null;
    const select = h("select", { class: "input", id: "rp-type" }, REPORTS.map((r) => h("option", { value: r.id }, r.label)));
    const description = h("p", { class: "hint" });
    const paramsBox = h("div", { class: "form-grid" });
    const formError = h("div", { class: "alert alert-error", role: "alert", hidden: true });
    const runBtn = h("button", { class: "btn btn-primary", type: "submit" }, "Run report");
    const output = h("div", { class: "mt", "aria-live": "polite" });
    let controls = [];

    async function setReport() {
        const report = REPORTS.find((r) => r.id === select.value);
        description.textContent = report.description;
        output.replaceChildren();
        formError.hidden = true;
        if (report.params.some((p) => p.needs === "students") && !studentItems) {
            try {
                const members = await api.get("/api/members");
                studentItems = members.filter((m) => m.memberType === "Student")
                    .map((m) => ({ id: m.memberID, label: `${m.memberID} \u00B7 ${m.name} (${m.registrationNo})` }));
            } catch (err) { formError.textContent = `Could not load students: ${err.message}`; formError.hidden = false; studentItems = []; }
        }
        controls = report.params.map((p) => makeField({ ...p, items: p.needs === "students" ? studentItems : undefined }));
        paramsBox.replaceChildren(...controls.map((c) => c.el));
        paramsBox.hidden = controls.length === 0;
    }

    const form = h("form", { class: "form-panel", novalidate: true, onsubmit: async (e) => {
        e.preventDefault();
        formError.hidden = true;
        const bad = controls.filter((c) => !c.validate());
        if (bad.length) { bad[0].input.focus(); return; }
        const report = REPORTS.find((r) => r.id === select.value);
        const values = Object.fromEntries(controls.map((c) => [c.spec.name, c.spec.type === "number" ? Number(c.getValue() || 0) : c.getValue()]));
        runBtn.disabled = true; runBtn.textContent = "Running\u2026";
        output.replaceChildren(loadingView("Running report\u2026"));
        try {
            const parts = await report.run(values);
            if (isCurrent()) output.replaceChildren(...parts.filter(Boolean));
        } catch (err) {
            output.replaceChildren(errorView(err.message));
        } finally {
            runBtn.disabled = false; runBtn.textContent = "Run report";
        }
    } },
        formError,
        h("div", { class: "field", style: "max-width:360px" }, h("label", { for: "rp-type" }, "Report"), select, description),
        h("div", { class: "mt" }, paramsBox),
        h("div", { class: "mt" }, runBtn));

    select.addEventListener("change", setReport);
    root.append(form, output);
    await setReport();
}
