import _sodium from 'libsodium-wrappers-sumo';

let sodium = null;

export async function initCrypto() {
    await _sodium.ready;
    sodium = _sodium;
}

export function generateKey() {
    if (!sodium) throw new Error('Crypto not initialized. Call initCrypto() first.');
    return sodium.crypto_aead_xchacha20poly1305_ietf_keygen();
}

export function encryptChunk(plaintext, key) {
    if (!sodium) throw new Error('Crypto not initialized. Call initCrypto() first.');
    const nonce = sodium.randombytes_buf(sodium.crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
    const ciphertext = sodium.crypto_aead_xchacha20poly1305_ietf_encrypt(
        plaintext,
        null,
        null,
        nonce,
        key,
    );
    const result = new Uint8Array(nonce.length + ciphertext.length);
    result.set(nonce, 0);
    result.set(ciphertext, nonce.length);
    return result;
}

export function decryptChunk(data, key) {
    if (!sodium) throw new Error('Crypto not initialized. Call initCrypto() first.');
    const nonceLen = sodium.crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
    const nonce = data.slice(0, nonceLen);
    const ciphertext = data.slice(nonceLen);
    return sodium.crypto_aead_xchacha20poly1305_ietf_decrypt(
        null,
        ciphertext,
        null,
        nonce,
        key,
    );
}

export function keyToBase64url(key) {
    if (!sodium) throw new Error('Crypto not initialized. Call initCrypto() first.');
    return sodium.to_base64(key, sodium.base64_variants.URLSAFE_NO_PADDING);
}

export function base64urlToKey(str) {
    if (!sodium) throw new Error('Crypto not initialized. Call initCrypto() first.');
    return sodium.from_base64(str, sodium.base64_variants.URLSAFE_NO_PADDING);
}
