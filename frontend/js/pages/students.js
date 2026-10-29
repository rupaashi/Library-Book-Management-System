import { api } from "../api.js";
import { h, pageHeader, mountAsync, dataTable, statusBadge, formDialog, confirmDialog, infoDialog, toast, orDash } from "../ui.js";

export async function studentsPage(root, isCurrent) {
    let members = [];
    const filters = { q: "", type: "", status: "active" };

    root.append(pageHeader({
        title: "Students", description: "Register library members and review their accounts.",
        actions: h("button", { class: "btn btn-primary", type: "button", onclick: addMember }, "Add student"),
    }));

    const search = h("input", { class: "input", id: "st-q", type: "search", placeholder: "Name, registration number, email or ID", oninput: (e) => { filters.q = e.target.value; draw(); } });
    const type = h("select", { class: "input", id: "st-t", onchange: (e) => { filters.type = e.target.value; draw(); } },
        h("option", { value: "" }, "All types"), h("option", { value: "Student" }, "Student"), h("option", { value: "Faculty" }, "Faculty"));
    const status = h("select", { class: "input", id: "st-s", onchange: (e) => { filters.status = e.target.value; draw(); } },
        h("option", { value: "active" }, "Active"), h("option", { value: "inactive" }, "Inactive"), h("option", { value: "" }, "All"));

    root.append(h("div", { class: "toolbar" },
        h("div", { class: "field grow" }, h("label", { for: "st-q" }, "Search"), search),
        h("div", { class: "field" }, h("label", { for: "st-t" }, "Member type"), type),
        h("div", { class: "field" }, h("label", { for: "st-s" }, "Account status"), status)));

    const results = h("div");
    root.append(results);

    async function load() {
        await mountAsync(results, () => api.get("/api/members"), (data) => { members = data; return buildTable(); });
    }

    function filtered() {
        const q = filters.q.toLowerCase().trim();
        return members.filter((m) => {
            if (filters.type && m.memberType !== filters.type) return false;
            if (filters.status === "active" && !m.active) return false;
            if (filters.status === "inactive" && m.active) return false;
            if (!q) return true;
            return [m.memberID, m.registrationNo, m.name, m.email].some((v) => String(v ?? "").toLowerCase().includes(q));
        });
    }

    function buildTable() {
        const rows = filtered();
        return h("div", {},
            h("p", { class: "count", role: "status" }, `${rows.length} of ${members.length} member(s)`),
            dataTable({
                caption: "Library members", rows, emptyTitle: "No students match your search",
                emptyText: "Try a different search or change the filters.",
                columns: [
                    { key: "memberID", label: "ID", align: "right" },
                    { key: "registrationNo", label: "Registration no." },
                    { key: "name", label: "Name" },
                    { key: "email", label: "Email", muted: true, render: (m) => orDash(m.email) },
                    { key: "phone", label: "Phone", muted: true, render: (m) => orDash(m.phone) },
                    { key: "memberType", label: "Type" },
                    { label: "Status", sortValue: (m) => (m.active ? "Active" : "Inactive"), render: (m) => statusBadge(m.active ? "ACTIVE" : "INACTIVE") },
                    { label: "Actions", sortable: false, render: (m) => h("div", { class: "actions" },
                        h("button", { class: "btn btn-secondary btn-sm", type: "button", "aria-label": `View ${m.name}`, onclick: () => view(m) }, "View"),
                        m.active ? h("button", { class: "btn btn-danger-outline btn-sm", type: "button", "aria-label": `Deactivate ${m.name}`, onclick: () => deactivate(m) }, "Deactivate") : null) },
                ],
            }));
    }

    function draw() {
        if (!members.length) return;
        results.replaceChildren(buildTable());
    }

    async function addMember() {
        const saved = await formDialog({
            title: "Add student", submitLabel: "Register",
            fields: [
                { name: "registrationNo", label: "Registration number", required: true, maxlength: 30 },
                { name: "name", label: "Full name", required: true, maxlength: 100 },
                { name: "memberType", label: "Member type", type: "select", required: true, options: ["Student", "Faculty"], hint: "Only students can borrow books." },
                { name: "email", label: "Email", type: "email", maxlength: 100, hint: "Optional." },
                { name: "phone", label: "Phone", type: "tel", maxlength: 20, hint: "Optional. Digits, spaces, + and - only.",
                    validate: (v) => (/^[+\d][\d\s-]{6,18}$/.test(v) ? null : "Enter a valid phone number (7 to 19 digits).") },
            ],
            onSubmit: async (v) => { const res = await api.post("/api/members", v); toast(`${res.message} (ID ${res.memberID})`); },
        });
        if (saved && isCurrent()) load();
    }

    function view(m) {
        infoDialog({ title: m.name, rows: [
            ["Member ID", m.memberID], ["Registration no.", m.registrationNo], ["Type", m.memberType],
            ["Email", orDash(m.email)], ["Phone", orDash(m.phone)], ["Status", m.active ? "Active" : "Inactive"],
        ] });
    }

    async function deactivate(m) {
        const ok = await confirmDialog({
            title: "Deactivate this account?", danger: true, confirmLabel: "Deactivate",
            message: `${m.name} will no longer be able to borrow or reserve books. Their history is kept.`,
        });
        if (!ok) return;
        try { toast((await api.post(`/api/members/${m.memberID}/deactivate`)).message); load(); }
        catch (err) { toast(err.message, "error"); }
    }

    await load();
}
