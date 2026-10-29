import { api, getConfig } from "../api.js";
import { h, pageHeader, dataTable, statusBadge, toast, fmtDate, money, sumMoney, orDash, confirmDialog, loadingView, errorView } from "../ui.js";

export async function finesPage(root, isCurrent) {
    const cfg = await getConfig().catch(() => null);
    root.append(pageHeader({
        title: "Fines",
        description: `Fines are created when a book is returned late${cfg ? ` (\u20B9${cfg.finePerDay} per day)` : ""} and are paid in full. Unpaid fines block borrowing, renewing and reserving.`,
    }));

    const filters = { status: "unpaid", q: "" };
    const status = h("select", { class: "input", id: "fn-s", onchange: (e) => { filters.status = e.target.value; load(); } },
        h("option", { value: "unpaid" }, "Outstanding (unpaid)"), h("option", { value: "paid" }, "Paid"), h("option", { value: "all" }, "All fines"));
    const search = h("input", { class: "input", id: "fn-q", type: "search", placeholder: "Student, book, fine or loan ID", oninput: (e) => { filters.q = e.target.value; draw(); } });
    const results = h("div");
    let rows = [];

    root.append(h("div", { class: "toolbar" },
        h("div", { class: "field" }, h("label", { for: "fn-s" }, "Show"), status),
        h("div", { class: "field grow" }, h("label", { for: "fn-q" }, "Search"), search)), results);

    async function load() {
        results.replaceChildren(loadingView());
        try { rows = await api.get(`/api/fines?status=${filters.status}`); if (isCurrent()) draw(true); }
        catch (err) { results.replaceChildren(errorView(err.message, load)); }
    }

    function draw(force) {
        if (!force && results.querySelector(".state.error")) return;
        const q = filters.q.toLowerCase().trim();
        const shown = rows.filter((f) => !q || [f.fineID, f.loanID, f.memberID, f.memberName, f.title].some((v) => String(v ?? "").toLowerCase().includes(q)));
        const unpaid = shown.filter((f) => f.status === "UNPAID");
        results.replaceChildren(
            h("p", { class: "count", role: "status" }, `${shown.length} fine(s)`
                + (unpaid.length ? ` \u00B7 ${money(sumMoney(unpaid.map((f) => f.amount)))} outstanding across ${unpaid.length}` : "")),
            dataTable({
                caption: "Fines", rows: shown, emptyTitle: filters.status === "unpaid" ? "No outstanding fines" : "No fines found",
                emptyText: filters.status === "unpaid" ? "Every fine has been paid." : "Change the filter to see other fines.",
                columns: [
                    { key: "fineID", label: "Fine", align: "right" },
                    { key: "memberName", label: "Student", render: (f) => h("span", {}, f.memberName, h("br"), h("span", { class: "hint" }, `ID ${f.memberID}`)) },
                    { key: "title", label: "Book" },
                    { key: "loanID", label: "Loan", align: "right" },
                    { key: "amount", label: "Amount", align: "right", render: (f) => money(f.amount) },
                    { label: "Status", sortValue: (f) => f.status, render: (f) => statusBadge(f.status) },
                    { key: "createdAt", label: "Created", render: (f) => fmtDate(f.createdAt) },
                    { key: "paidAt", label: "Paid on", render: (f) => fmtDate(f.paidAt) },
                    { label: "Actions", sortable: false, render: (f) => f.status === "UNPAID"
                        ? h("button", { class: "btn btn-primary btn-sm", type: "button", "aria-label": `Mark fine ${f.fineID} as paid`, onclick: () => pay(f) }, "Mark as paid") : orDash("") },
                ],
            }));
    }

    async function pay(f) {
        const ok = await confirmDialog({
            title: "Record full payment?", confirmLabel: `Record ${money(f.amount)} payment`,
            message: `${f.memberName} pays ${money(f.amount)} for fine ${f.fineID} (\u201C${f.title}\u201D). Partial payments are not supported.`,
        });
        if (!ok) return;
        try { toast((await api.post(`/api/fines/${f.fineID}/pay`)).message); if (isCurrent()) load(); }
        catch (err) { toast(err.message, "error"); load(); }
    }

    await load();
}
