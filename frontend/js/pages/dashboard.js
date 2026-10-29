import { api } from "../api.js";
import { h, pageHeader, mountAsync, dataTable, fmtDate, money } from "../ui.js";

const METRICS = [
    ["Book titles", (d) => d.titles, () => "Active titles in the catalogue"],
    ["Total copies", (d) => d.totalCopies, () => "Across active titles"],
    ["Available copies", (d) => d.availableCopies, () => "On the shelf now"],
    ["Currently issued", (d) => d.issuedLoans, () => "Loans not yet returned"],
    ["Overdue loans", (d) => d.overdueLoans, () => "Past their due date"],
    ["Active students", (d) => d.activeStudents, () => "Allowed to borrow"],
    ["Pending reservations", (d) => (d.waitingReservations == null ? null : d.waitingReservations + (d.readyReservations || 0)),
        (d) => `${d.waitingReservations ?? 0} waiting, ${d.readyReservations ?? 0} ready to collect`],
    ["Outstanding fines", (d) => (d.unpaidAmount == null ? null : money(d.unpaidAmount)),
        (d) => `${d.unpaidFines ?? 0} unpaid fine(s)`],
];

export async function dashboardPage(root) {
    root.append(pageHeader({ title: "Dashboard", description: "A summary of the library, read live from the database." }));
    const body = h("div");
    root.append(body);

    await mountAsync(body, () => api.get("/api/dashboard"), (d) => {
        const metrics = h("dl", { class: "metrics" }, METRICS.map(([label, get, note]) => {
            const value = get(d);
            const missing = value == null;
            return h("div", { class: `metric${missing ? " na" : ""}` },
                h("dt", {}, label),
                h("dd", {}, missing ? "Unavailable" : (typeof value === "number" ? value.toLocaleString("en-IN") : value),
                    missing ? null : h("span", { class: "note" }, note(d))));
        }));

        const recent = d.recent || [];
        return h("div", {}, metrics,
            h("section", { class: "section", "aria-labelledby": "recentH" },
                h("h2", { id: "recentH" }, "Recent issue and return activity"),
                h("p", { class: "help" }, "The latest eight events, newest first."),
                dataTable({
                    caption: "Recent activity", rows: recent, pageSize: 8,
                    emptyTitle: "No activity yet", emptyText: "Issued and returned books will appear here.",
                    columns: [
                        { key: "eventDate", label: "Date", render: (r) => fmtDate(r.eventDate) },
                        { key: "event", label: "Event" },
                        { key: "title", label: "Book" },
                        { key: "memberName", label: "Student" },
                        { key: "loanID", label: "Loan", align: "right" },
                    ],
                })));
    });
}
