import { API_BASE } from "./config.js";

export class ApiError extends Error {
    constructor(message, status = 0) {
        super(message);
        this.status = status;
    }
}

async function request(method, path, body) {
    let response;
    try {
        response = await fetch(API_BASE + path, {
            method,
            headers: body !== undefined ? { "Content-Type": "application/json" } : {},
            body: body !== undefined ? JSON.stringify(body) : undefined,
        });
    } catch {
        throw new ApiError("Cannot reach the server. Check that the backend is running and try again.");
    }

    let data = null;
    try {
        data = await response.json();
    } catch {
        /* non-JSON response */
    }

    if (!response.ok) {
        throw new ApiError(
            (data && data.error) || `The server returned an error (${response.status}).`,
            response.status
        );
    }
    return data;
}

export const api = {
    get: (path) => request("GET", path),
    post: (path, body = {}) => request("POST", path, body),
    patch: (path, body = {}) => request("PATCH", path, body),
};

let configPromise = null;
/** Library policy (loan days, fine per day, renewals, hold days) from the server. */
export function getConfig() {
    if (!configPromise) {
        configPromise = api.get("/api/config").catch((err) => {
            configPromise = null;
            throw err;
        });
    }
    return configPromise;
}
