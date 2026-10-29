// App shell: hash router, navigation, mobile sidebar.
import { h } from "./ui.js";
import { dashboardPage } from "./pages/dashboard.js";
import { booksPage } from "./pages/books.js";
import { studentsPage } from "./pages/students.js";
import { circulationPage } from "./pages/circulation.js";
import { reservationsPage } from "./pages/reservations.js";
import { finesPage } from "./pages/fines.js";
import { reportsPage } from "./pages/reports.js";

const icon = (paths) => {
    const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    for (const [k, v] of Object.entries({ width: 18, height: 18, viewBox: "0 0 24 24", fill: "none",
        stroke: "currentColor", "stroke-width": 1.8, "stroke-linecap": "round", "stroke-linejoin": "round", "aria-hidden": "true" }))
        svg.setAttribute(k, v);
    svg.innerHTML = paths;
    return svg;
};

const PAGES = [
    { path: "dashboard", label: "Dashboard", page: dashboardPage, icon: '<rect x="3" y="3" width="7" height="9"/><rect x="14" y="3" width="7" height="5"/><rect x="14" y="12" width="7" height="9"/><rect x="3" y="16" width="7" height="5"/>' },
    { path: "books", label: "Books", page: booksPage, icon: '<path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20"/><path d="M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2z"/>' },
    { path: "students", label: "Students", page: studentsPage, icon: '<path d="M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><path d="M23 21v-2a4 4 0 0 0-3-3.87"/><path d="M16 3.13a4 4 0 0 1 0 7.75"/>' },
    { path: "circulation", label: "Issue and Return", page: circulationPage, icon: '<path d="M17 1l4 4-4 4"/><path d="M3 11V9a4 4 0 0 1 4-4h14"/><path d="M7 23l-4-4 4-4"/><path d="M21 13v2a4 4 0 0 1-4 4H3"/>' },
    { path: "reservations", label: "Reservations", page: reservationsPage, icon: '<path d="M19 21l-7-5-7 5V5a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2z"/>' },
    { path: "fines", label: "Fines", page: finesPage, icon: '<path d="M6 3h12"/><path d="M6 8h12"/><path d="M6 13l8.5 8"/><path d="M6 13h3a4 4 0 0 0 0-8"/>' },
    { path: "reports", label: "Reports", page: reportsPage, icon: '<line x1="18" y1="20" x2="18" y2="10"/><line x1="12" y1="20" x2="12" y2="4"/><line x1="6" y1="20" x2="6" y2="14"/>' },
];

const app = document.getElementById("app");
const main = document.getElementById("main");
const navToggle = document.getElementById("navToggle");
const scrim = document.getElementById("scrim");
let renderToken = 0;
let firstRoute = true;

document.getElementById("navList").append(...PAGES.map((p) =>
    h("li", {}, (() => {
        const a = h("a", { class: "nav-link", href: `#/${p.path}`, "data-path": p.path }, p.label);
        a.prepend(icon(p.icon));
        return a;
    })())));

function setNav(open) {
    app.classList.toggle("nav-open", open);
    scrim.hidden = !open;
    navToggle.setAttribute("aria-expanded", String(open));
    navToggle.setAttribute("aria-label", open ? "Close navigation menu" : "Open navigation menu");
}
navToggle.addEventListener("click", () => setNav(!app.classList.contains("nav-open")));
scrim.addEventListener("click", () => setNav(false));
document.addEventListener("keydown", (e) => { if (e.key === "Escape" && app.classList.contains("nav-open")) { setNav(false); navToggle.focus(); } });

async function route() {
    const path = location.hash.replace(/^#\/?/, "") || "dashboard";
    const entry = PAGES.find((p) => p.path === path) || PAGES[0];
    const token = ++renderToken;

    document.title = `${entry.label} \u2013 Library Book Management System`;
    document.getElementById("crumbs").textContent = entry.label;
    document.querySelectorAll(".nav-link").forEach((a) => {
        if (a.dataset.path === entry.path) a.setAttribute("aria-current", "page");
        else a.removeAttribute("aria-current");
    });
    setNav(false);

    main.replaceChildren();
    try {
        await entry.page(main, () => token === renderToken);
    } catch (err) {
        main.replaceChildren(h("div", { class: "state error", role: "alert" }, h("strong", {}, "This page could not be displayed"), err.message));
    }
    window.scrollTo(0, 0);
    // After navigating, move focus to the page so keyboard and screen-reader
    // users start at the new content. Not on first load (keeps the skip link first).
    if (!firstRoute) main.focus({ preventScroll: true });
    firstRoute = false;
}

window.addEventListener("hashchange", route);
route();
