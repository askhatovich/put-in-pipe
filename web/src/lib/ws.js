let socket = null;
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

export function connect() {
    return new Promise((resolve, reject) => {
        const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
        const url = `${proto}//${location.host}/api/ws`;

        socket = new WebSocket(url);
        socket.binaryType = 'arraybuffer';

        socket.onopen = () => {
            console.log('[WS] connected to', url);
            dispatch('open', null);
            resolve();
        };

        socket.onerror = (err) => {
            console.error('[WS] error:', err);
            dispatch('error', err);
            reject(err);
        };

        socket.onclose = (ev) => {
            console.log('[WS] closed:', ev.code, ev.reason);
            dispatch('close', { code: ev.code, reason: ev.reason });
            socket = null;
        };

        socket.onmessage = (ev) => {
            if (ev.data instanceof ArrayBuffer) {
                dispatch('binary', new Uint8Array(ev.data));
                return;
            }
            try {
                const msg = JSON.parse(ev.data);
                if (msg.event) {
                    console.log('[WS] event:', msg.event, msg.data);
                    dispatch(msg.event, msg);
                }
            } catch {
                // ignore non-JSON text messages
            }
        };
    });
}

export function disconnect() {
    if (socket) {
        socket.close();
        socket = null;
    }
}

export function sendAction(action, data) {
    if (!socket || socket.readyState !== WebSocket.OPEN) {
        console.error('sendAction failed: WebSocket not connected, action:', action);
        return false;
    }
    socket.send(JSON.stringify({ action, data }));
    return true;
}

export function sendBinary(data) {
    if (!socket || socket.readyState !== WebSocket.OPEN) {
        console.error('sendBinary failed: WebSocket not connected');
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
