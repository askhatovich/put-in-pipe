const SUPPORTED_ENCRYPTIONS = ['xchacha20-poly1305'];

export function parseAnchor() {
    const hash = location.hash.slice(1);
    if (!hash) return null;
    const params = new URLSearchParams(hash);
    const id = params.get('id');
    const encryption = params.get('encryption');
    const key = params.get('key');
    if (!id || !key) return null;
    if (!encryption || !SUPPORTED_ENCRYPTIONS.includes(encryption)) {
        return { id, key, encryption, error: `Unsupported encryption: ${encryption || 'none'}` };
    }
    return { id, encryption, key, error: null };
}

export function buildAnchor(sessionId, encryption, keyBase64url) {
    return `${location.origin}/#id=${sessionId}&encryption=${encryption}&key=${keyBase64url}`;
}

export function clearAnchor() {
    history.replaceState(null, '', location.pathname + location.search);
}
