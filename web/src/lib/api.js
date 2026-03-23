async function parseResponse(res) {
    const text = await res.text();
    try {
        return { status: res.status, data: JSON.parse(text), error: null };
    } catch {
        // Server returned non-JSON (plain text error message)
        return { status: res.status, data: text, error: null };
    }
}

export async function getStatistics() {
    try {
        const res = await fetch('/api/statistics/current');
        return await parseResponse(res);
    } catch (err) {
        return { status: 0, data: null, error: err.message };
    }
}

export async function getMyInfo() {
    try {
        const res = await fetch('/api/me/info');
        if (res.status === 401) return { status: 401, data: null, error: null };
        return await parseResponse(res);
    } catch (err) {
        return { status: 0, data: null, error: err.message };
    }
}

export async function leave() {
    try {
        const res = await fetch('/api/me/leave', { method: 'POST' });
        return await parseResponse(res);
    } catch (err) {
        return { status: 0, data: null, error: err.message };
    }
}

export async function requestIdentity(name) {
    try {
        const res = await fetch(`/api/identity/request?name=${encodeURIComponent(name)}`);
        return await parseResponse(res);
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
        return await parseResponse(res);
    } catch (err) {
        return { status: 0, data: null, error: err.message };
    }
}

export async function createSession() {
    try {
        const res = await fetch('/api/session/create', { method: 'POST' });
        return await parseResponse(res);
    } catch (err) {
        return { status: 0, data: null, error: err.message };
    }
}

export async function joinSession(id) {
    try {
        const res = await fetch(`/api/session/join?id=${encodeURIComponent(id)}`);
        return await parseResponse(res);
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
        return await parseResponse(res);
    } catch (err) {
        return { status: 0, data: null, error: err.message };
    }
}

export async function downloadChunk(id) {
    try {
        const res = await fetch(`/api/session/chunk?id=${encodeURIComponent(id)}`);
        if (!res.ok) {
            const text = await res.text();
            return { status: res.status, data: null, error: text || `HTTP ${res.status}` };
        }
        const data = await res.arrayBuffer();
        return { status: res.status, data, error: null };
    } catch (err) {
        return { status: 0, data: null, error: err.message };
    }
}
