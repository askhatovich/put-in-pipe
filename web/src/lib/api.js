export async function getStatistics() {
    try {
        const res = await fetch('/api/statistics/current');
        const data = await res.json();
        return { status: res.status, data, error: null };
    } catch (err) {
        return { status: 0, data: null, error: err.message };
    }
}

export async function getMyInfo() {
    try {
        const res = await fetch('/api/me/info');
        if (res.status === 401) return { status: 401, data: null, error: null };
        const data = await res.json();
        return { status: res.status, data, error: null };
    } catch (err) {
        return { status: 0, data: null, error: err.message };
    }
}

export async function leave() {
    try {
        const res = await fetch('/api/me/leave', { method: 'POST' });
        const data = await res.json().catch(() => null);
        return { status: res.status, data, error: null };
    } catch (err) {
        return { status: 0, data: null, error: err.message };
    }
}

export async function requestIdentity(name) {
    try {
        const res = await fetch(`/api/identity/request?name=${encodeURIComponent(name)}`);
        const data = await res.json();
        return { status: res.status, data, error: null };
    } catch (err) {
        return { status: 0, data: null, error: err.message };
    }
}

export async function confirmIdentity(payload) {
    try {
        const res = await fetch('/api/identity/confirmation', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload),
        });
        const data = await res.json();
        return { status: res.status, data, error: null };
    } catch (err) {
        return { status: 0, data: null, error: err.message };
    }
}

export async function createSession() {
    try {
        const res = await fetch('/api/session/create', { method: 'POST' });
        const data = await res.json();
        return { status: res.status, data, error: null };
    } catch (err) {
        return { status: 0, data: null, error: err.message };
    }
}

export async function joinSession(id) {
    try {
        const res = await fetch(`/api/session/join?id=${encodeURIComponent(id)}`);
        const data = await res.json();
        return { status: res.status, data, error: null };
    } catch (err) {
        return { status: 0, data: null, error: err.message };
    }
}

export async function uploadChunk(binaryData) {
    try {
        const res = await fetch('/api/session/chunk', {
            method: 'POST',
            headers: { 'Content-Type': 'application/octet-stream' },
            body: binaryData,
        });
        const data = await res.json().catch(() => null);
        return { status: res.status, data, error: null };
    } catch (err) {
        return { status: 0, data: null, error: err.message };
    }
}

export async function downloadChunk(id) {
    try {
        const res = await fetch(`/api/session/chunk?id=${encodeURIComponent(id)}`);
        if (!res.ok) return { status: res.status, data: null, error: `HTTP ${res.status}` };
        const data = await res.arrayBuffer();
        return { status: res.status, data, error: null };
    } catch (err) {
        return { status: 0, data: null, error: err.message };
    }
}
