// Shared UI toolkit: element builder, formatting, form fields, dialogs,
// toasts, status labels, data tables and load/empty/error states.

let uidCounter = 0;
export const uid = (p = "f") => `${p}${++uidCounter}`;

/* ------------------------------------------------------------ DOM builder */
export function h(tag, attrs = {}, ...children) {
    const node = document.createElement(tag);
    for (const [key, value] of Object.entries(attrs || {})) {
        if (value == null || value === false) continue;
        if (key === "class") node.className = value;
        else if (key === "text") node.textContent = value;
        else if (key.startsWith("on") && typeof value === "function")
            node.addEventListener(key.slice(2).toLowerCase(), value);
        else if (value === true) node.setAttribute(key, "");
        else node.setAttribute(key, value);
    }
    append(node, children);
    return node;
}

function append(node, children) {
    for (const child of children.flat(Infinity)) {
        if (child == null || child === false) continue;
        node.append(child instanceof Node ? child : document.createTextNode(String(child)));
    }
}

/* ------------------------------------------------------------ formatting */
const DASH = "\u2014";

export function fmtDate(value) {
    if (!value) return DASH;
    const m = String(value).match(/^(\d{4})-(\d{2})-(\d{2})(?:[ T](\d{2}):(\d{2}))?/);
    if (!m) return String(value);
    const date = new Date(+m[1], +m[2] - 1, +m[3]);
    let text = date.toLocaleDateString("en-IN", { day: "numeric", month: "short", year: "numeric" });
    if (m[4] !== undefined) text += `, ${m[4]}:${m[5]}`;
    return text;
}

export function money(value) {
    if (value == null || value === "") return DASH;
    const n = Number(value);
    if (Number.isNaN(n)) return DASH;
    return "\u20B9" + n.toLocaleString("en-IN", { minimumFractionDigits: 2, maximumFractionDigits: 2 });
}

/** Sum decimal strings exactly (in paise), so totals never drift. */
export function sumMoney(values) {
    const paise = values.reduce((total, v) => total + Math.round(parseFloat(v) * 100), 0);
    return paise / 100;
}

export const orDash = (v) => (v == null || v === "" ? DASH : v);

/* ------------------------------------------------------------ status labels */
const STATUS = {
    ACTIVE: ["Active", "ok"], OVERDUE: ["Overdue", "danger"], RETURNED: ["Returned", "neutral"],
    WAITING: ["Waiting", "warn"], READY: ["Ready", "ok"], FULFILLED: ["Fulfilled", "neutral"],
    CANCELLED: ["Cancelled", "neutral"], EXPIRED: ["Expired", "neutral"],
    PAID: ["Paid", "ok"], UNPAID: ["Unpaid", "warn"],
    AVAILABLE: ["Available", "ok"], UNAVAILABLE: ["Unavailable", "warn"],
    INACTIVE: ["Inactive", "neutral"],
};

export function badge(text, kind = "neutral") {
    return h("span", { class: `badge badge-${kind}` }, text);
}

export function statusBadge(code) {
    const [label, kind] = STATUS[String(code).toUpperCase()] || [String(code), "neutral"];
    return badge(label, kind);
}

/* ------------------------------------------------------------ states */
export function loadingView(text = "Loading\u2026") {
    return h("div", { class: "state", role: "status" }, h("span", { class: "spinner", "aria-hidden": "true" }), text);
}

export function emptyView(title, text = "") {
    return h("div", { class: "state" }, h("strong", {}, title), text);
}

export function errorView(message, onRetry) {
    return h("div", { class: "state error", role: "alert" },
        h("strong", {}, "Something went wrong"), message,
        onRetry ? h("div", {}, h("button", { class: "btn btn-secondary", type: "button", onclick: onRetry }, "Try again")) : null);
}

/** Show a loading state, run `loader`, then render its result (or an error with Retry). */
export async function mountAsync(container, loader, render) {
    container.replaceChildren(loadingView());
    try {
        const data = await loader();
        container.replaceChildren(render(data));
    } catch (err) {
        container.replaceChildren(errorView(err.message, () => mountAsync(container, loader, render)));
    }
}

export function pageHeader({ title, description, actions }) {
    return h("div", { class: "page-head" },
        h("div", {}, h("h1", {}, title), description ? h("p", {}, description) : null),
        actions ? h("div", { class: "actions" }, actions) : null);
}

/* ------------------------------------------------------------ toasts */
export function toast(message, type = "success") {
    const region = document.getElementById("toasts");
    const node = h("div", { class: `toast ${type}`, role: type === "error" ? "alert" : "status" },
        h("p", {}, message),
        h("button", { type: "button", "aria-label": "Dismiss message", onclick: () => node.remove() }, "\u00D7"));
    region.append(node);
    setTimeout(() => node.remove(), type === "error" ? 10000 : 6000);
}

/* ------------------------------------------------------------ form fields */
/**
 * spec: { name, label, type: text|number|email|tel|select|lookup|date, required, hint,
 *         value, options (select), items [{id,label}] (lookup), min, max, maxlength,
 *         validate(value) -> error text | null }
 */
export function makeField(spec) {
    const id = uid("fld");
    const hintId = `${id}-hint`;
    const errId = `${id}-err`;
    const type = spec.type || "text";
    let input;
    let listEl = null;

    if (type === "select") {
        input = h("select", { class: "input", id, name: spec.name },
            (spec.options || []).map((o) => {
                const opt = typeof o === "object" ? o : { value: o, label: o };
                return h("option", { value: opt.value }, opt.label);
            }));
    } else {
        input = h("input", {
            class: "input", id, name: spec.name, autocomplete: "off",
            type: type === "lookup" ? "text" : type,
            min: spec.min, max: spec.max, maxlength: spec.maxlength, placeholder: spec.placeholder,
            list: type === "lookup" ? `${id}-list` : null,
        });
        if (type === "lookup") {
            listEl = h("datalist", { id: `${id}-list` },
                (spec.items || []).map((item) => h("option", { value: item.label })));
        }
    }
    if (spec.value != null && spec.value !== "") input.value = spec.value;

    const errorEl = h("div", { class: "error-text", id: errId, hidden: true });
    const describedBy = [spec.hint ? hintId : null, errId].filter(Boolean).join(" ");
    input.setAttribute("aria-describedby", describedBy);
    if (spec.required) input.setAttribute("aria-required", "true");

    const hintEl = spec.hint ? h("div", { class: "hint", id: hintId }, spec.hint) : null;
    const el = h("div", { class: "field" },
        h("label", { for: id }, spec.label, spec.required ? h("span", { class: "req", "aria-hidden": "true" }, "*") : null,
            spec.required ? h("span", { class: "sr-only" }, " (required)") : null),
        input, listEl, hintEl, errorEl);

    const resolveLookup = () => {
        const text = input.value.trim();
        if (!text) return null;
        const exact = (spec.items || []).find((i) => i.label === text);
        if (exact) return exact.id;
        if (/^\d+$/.test(text)) return (spec.items || []).find((i) => String(i.id) === text)?.id ?? null;
        return null;
    };

    const field = {
        spec, el, input,
        /** Replace the suggestions of a lookup field (after the data changed). */
        setItems(items) {
            spec.items = items;
            if (listEl) listEl.replaceChildren(...items.map((item) => h("option", { value: item.label })));
        },
        getValue() {
            if (type === "lookup") return resolveLookup();
            return input.value.trim();
        },
        setError(message) {
            errorEl.textContent = message || "";
            errorEl.hidden = !message;
            input.setAttribute("aria-invalid", message ? "true" : "false");
        },
        validate() {
            const raw = input.value.trim();
            const name = spec.label.toLowerCase();
            let message = null;
            if (!raw) {
                if (spec.required) message = `${spec.label} is required.`;
            } else if (type === "lookup" && resolveLookup() == null) {
                message = `Choose a ${name} from the suggestions.`;
            } else if (type === "number") {
                const n = Number(raw);
                if (!Number.isInteger(n)) message = `${spec.label} must be a whole number.`;
                else if (spec.min != null && n < spec.min) message = `${spec.label} must be at least ${spec.min}.`;
                else if (spec.max != null && n > spec.max) message = `${spec.label} must be at most ${spec.max}.`;
            } else if (type === "email" && !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(raw)) {
                message = "Enter a valid email address, for example name@college.edu.";
            }
            if (!message && raw && spec.validate) message = spec.validate(field.getValue());
            field.setError(message);
            return !message;
        },
    };
    input.addEventListener("input", () => { if (errorEl.hidden === false) field.validate(); });
    return field;
}

/* ------------------------------------------------------------ dialogs */
function openDialog(dlg) {
    document.body.append(dlg);
    dlg.addEventListener("close", () => dlg.remove());
    dlg.showModal();
}

/** Modal form. onSubmit(values) may throw; the message is shown and the input is kept. */
export function formDialog({ title, intro, fields, submitLabel = "Save", initial = {}, onSubmit }) {
    return new Promise((resolve) => {
        const titleId = uid("dlg");
        const controls = fields.map((f) => makeField({ ...f, value: initial[f.name] ?? f.value }));
        const errBox = h("div", { class: "alert alert-error", role: "alert", hidden: true });
        const submit = h("button", { class: "btn btn-primary", type: "submit" }, submitLabel);
        const dlg = h("dialog", { "aria-labelledby": titleId });

        const form = h("form", {
            novalidate: true,
            onsubmit: async (event) => {
                event.preventDefault();
                errBox.hidden = true;
                const invalid = controls.filter((c) => !c.validate());
                if (invalid.length) { invalid[0].input.focus(); return; }
                const values = Object.fromEntries(controls.map((c) => [c.spec.name, c.getValue()]));
                submit.disabled = true;
                submit.textContent = "Saving\u2026";
                try {
                    await onSubmit(values);
                    dlg.close("saved");
                } catch (err) {
                    errBox.textContent = err.message;
                    errBox.hidden = false;
                    submit.disabled = false;
                    submit.textContent = submitLabel;
                }
            },
        },
            h("div", { class: "dlg-head" }, h("h2", { id: titleId }, title)),
            h("div", { class: "dlg-body" }, intro ? h("p", { class: "hint" }, intro) : null, errBox, controls.map((c) => c.el)),
            h("div", { class: "dlg-foot" },
                h("button", { class: "btn btn-secondary", type: "button", onclick: () => dlg.close() }, "Cancel"), submit));

        dlg.append(form);
        dlg.addEventListener("close", () => resolve(dlg.returnValue === "saved"));
        openDialog(dlg);
        controls[0]?.input.focus();
    });
}

/** Yes/no dialog. Resolves true when confirmed. Focus starts on Cancel. */
export function confirmDialog({ title, message, confirmLabel = "Confirm", danger = false }) {
    return new Promise((resolve) => {
        const titleId = uid("dlg");
        const cancel = h("button", { class: "btn btn-secondary", type: "button", onclick: () => dlg.close("no") }, "Cancel");
        const ok = h("button", { class: `btn ${danger ? "btn-danger" : "btn-primary"}`, type: "button", onclick: () => dlg.close("yes") }, confirmLabel);
        const dlg = h("dialog", { "aria-labelledby": titleId },
            h("div", { class: "dlg-head" }, h("h2", { id: titleId }, title)),
            h("div", { class: "dlg-body" }, h("p", {}, message)),
            h("div", { class: "dlg-foot" }, cancel, ok));
        dlg.addEventListener("close", () => resolve(dlg.returnValue === "yes"));
        openDialog(dlg);
        cancel.focus();
    });
}

/** Read-only details. rows: [[label, value], ...] */
export function infoDialog({ title, rows }) {
    const titleId = uid("dlg");
    const dlg = h("dialog", { "aria-labelledby": titleId },
        h("div", { class: "dlg-head" }, h("h2", { id: titleId }, title)),
        h("div", { class: "dlg-body" },
            h("dl", { class: "details" }, rows.map(([k, v]) => [h("dt", {}, k), h("dd", {}, v)]))),
        h("div", { class: "dlg-foot" },
            h("button", { class: "btn btn-secondary", type: "button", onclick: () => dlg.close() }, "Close")));
    openDialog(dlg);
}

/* ------------------------------------------------------------ data table */
const isNumeric = (v) => v !== "" && v != null && /^-?\d+(\.\d+)?$/.test(String(v).trim());

function compare(a, b) {
    const aNull = a == null || a === "", bNull = b == null || b === "";
    if (aNull || bNull) return aNull === bNull ? 0 : aNull ? 1 : -1;
    if (isNumeric(a) && isNumeric(b)) return parseFloat(a) - parseFloat(b);
    return String(a).localeCompare(String(b), undefined, { numeric: true, sensitivity: "base" });
}

/**
 * columns: [{ key, label, render(row), sortValue(row), align: "right", muted, sortable }]
 * Sorting and pagination run in the browser on the rows the API returned.
 */
export function dataTable({ columns, rows, pageSize = 15, caption = "Results", emptyTitle = "Nothing to show", emptyText = "", defaultSort }) {
    if (!rows.length) return emptyView(emptyTitle, emptyText);

    let sortIndex = defaultSort ? columns.findIndex((c) => c.key === defaultSort.key) : -1;
    let sortDir = defaultSort?.dir || "asc";
    let page = 0;
    const root = h("div");

    const valueOf = (col, row) => (col.sortValue ? col.sortValue(row) : row[col.key]);
    const sortable = (col) => col.sortable !== false && (col.key || col.sortValue);

    function draw() {
        let data = rows;
        if (sortIndex >= 0) {
            const col = columns[sortIndex];
            data = [...rows].sort((a, b) => (sortDir === "asc" ? 1 : -1) * compare(valueOf(col, a), valueOf(col, b)));
        }
        const pages = Math.max(1, Math.ceil(data.length / pageSize));
        page = Math.min(page, pages - 1);
        const slice = data.slice(page * pageSize, page * pageSize + pageSize);

        const head = h("tr", {}, columns.map((col, i) => {
            const active = i === sortIndex;
            const th = h("th", { scope: "col", class: col.align === "right" ? "num" : null,
                "aria-sort": sortable(col) ? (active ? (sortDir === "asc" ? "ascending" : "descending") : "none") : null });
            if (sortable(col)) {
                th.append(h("button", { type: "button", onclick: () => {
                    if (sortIndex === i) sortDir = sortDir === "asc" ? "desc" : "asc";
                    else { sortIndex = i; sortDir = "asc"; }
                    page = 0; draw();
                } }, col.label, h("span", { "aria-hidden": "true" }, active ? (sortDir === "asc" ? " \u25B2" : " \u25BC") : "")));
            } else th.append(col.label);
            return th;
        }));

        const body = slice.map((row) => h("tr", {}, columns.map((col) => {
            const content = col.render ? col.render(row) : orDash(row[col.key]);
            return h("td", { class: [col.align === "right" ? "num" : "", col.muted ? "sub" : ""].join(" ").trim() || null }, content);
        })));

        const table = h("table", {}, h("caption", { class: "sr-only" }, caption), h("thead", {}, head), h("tbody", {}, body));
        const parts = [h("div", { class: "table-wrap" }, table)];

        if (data.length > pageSize) {
            const from = page * pageSize + 1, to = Math.min(data.length, from + pageSize - 1);
            parts[0].append(h("div", { class: "pager" },
                h("span", {}, `Showing ${from}\u2013${to} of ${data.length}`),
                h("div", { class: "actions" },
                    h("button", { class: "btn btn-secondary btn-sm", type: "button", disabled: page === 0, onclick: () => { page--; draw(); } }, "Previous"),
                    h("button", { class: "btn btn-secondary btn-sm", type: "button", disabled: page >= pages - 1, onclick: () => { page++; draw(); } }, "Next"))));
        }
        root.replaceChildren(...parts);
    }
    draw();
    return root;
}

/* ------------------------------------------------------------ CSV export */
export function downloadCsv(filename, columns, rows) {
    const esc = (v) => `"${String(v ?? "").replace(/"/g, '""')}"`;
    const lines = [columns.map((c) => esc(c.label)).join(",")];
    for (const row of rows) lines.push(columns.map((c) => esc(c.csv ? c.csv(row) : row[c.key])).join(","));
    const blob = new Blob(["\uFEFF" + lines.join("\r\n")], { type: "text/csv;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    const link = h("a", { href: url, download: filename });
    document.body.append(link);
    link.click();
    link.remove();
    URL.revokeObjectURL(url);
}
