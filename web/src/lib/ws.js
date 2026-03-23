let socket = null;
let autoReconnect = false;
let reconnectAttempts = 0;
let reconnectTimer = null;
const MAX_RECONNECT_DELAY = 10000;
const handlers = new Map();

function getListeners(eventName) {
    if (!handlers.has(eventName)) {
        handlers.set(eventName, new Set());
    }
    return handlers.get(eventName);
}

function dispatch(eventName, data) {
    const listeners = handlers.get(eventName);
    if (listeners) {
        listeners.forEach(fn => fn(data));
    }
}

function getWsUrl() {
    const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
    return `${proto}//${location.host}/api/ws`;
}

function createSocket(url, resolve, reject) {
    socket = new WebSocket(url);
    socket.binaryType = 'arraybuffer';

    socket.onopen = () => {
        reconnectAttempts = 0;
        dispatch('open', null);
        if (resolve) resolve();
    };

    socket.onerror = (err) => {
        dispatch('error', err);
        if (reject) { reject(err); reject = null; }
    };

    socket.onclose = (ev) => {
        socket = null;

        // Normal close (1000) or intentional disconnect — no reconnect
        if (!autoReconnect || ev.code === 1000) {
            dispatch('close', { code: ev.code, reason: ev.reason });
            return;
        }

        // Abnormal close — try reconnect
        dispatch('reconnecting', { attempt: reconnectAttempts + 1 });
        scheduleReconnect(url);
    };

    socket.onmessage = (ev) => {
        if (ev.data instanceof ArrayBuffer) {
            dispatch('binary', new Uint8Array(ev.data));
            return;
        }
        try {
            const msg = JSON.parse(ev.data);
            if (msg.event) {
                dispatch(msg.event, msg);
            }
        } catch {
            // ignore non-JSON text messages
        }
    };
}

function scheduleReconnect(url) {
    reconnectAttempts++;
    const delay = Math.min(1000 * Math.pow(1.5, reconnectAttempts - 1), MAX_RECONNECT_DELAY);

    reconnectTimer = setTimeout(() => {
        reconnectTimer = null;
        if (!autoReconnect) return;
        createSocket(url, null, null);
    }, delay);
}

export function connect() {
    autoReconnect = true;
    reconnectAttempts = 0;
    return new Promise((resolve, reject) => {
        createSocket(getWsUrl(), resolve, reject);
    });
}

export function disconnect() {
    autoReconnect = false;
    if (reconnectTimer) {
        clearTimeout(reconnectTimer);
        reconnectTimer = null;
    }
    if (socket) {
        socket.close(1000, 'client disconnect');
        socket = null;
    }
}

export function sendAction(action, data) {
    if (!socket || socket.readyState !== WebSocket.OPEN) {
        return false;
    }
    socket.send(JSON.stringify({ action, data }));
    return true;
}

export function sendBinary(data) {
    if (!socket || socket.readyState !== WebSocket.OPEN) {
        return false;
    }
    socket.send(data);
    return true;
}

export function on(eventName, callback) {
    getListeners(eventName).add(callback);
}

export function off(eventName, callback) {
    const listeners = handlers.get(eventName);
    if (listeners) {
        listeners.delete(callback);
    }
}

export function isConnected() {
    return socket !== null && socket.readyState === WebSocket.OPEN;
}

// Wait until WS is connected (or already is)
export function waitForConnection(timeoutMs = 15000) {
    if (isConnected()) return Promise.resolve();
    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => {
            off('open', onOpen);
            reject(new Error('WS reconnect timeout'));
        }, timeoutMs);
        function onOpen() {
            clearTimeout(timer);
            off('open', onOpen);
            resolve();
        }
        on('open', onOpen);
    });
}
