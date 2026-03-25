# Encryption

## Algorithm

XChaCha20-Poly1305 AEAD via libsodium (browser: libsodium-wrappers).

- Key: 32 bytes (256-bit), random per session
- Nonce: 24 bytes (192-bit), random per chunk
- Auth tag: 16 bytes (128-bit)

## Chunk Wire Format

```
[nonce: 24 bytes][ciphertext: plaintext_size bytes][tag: 16 bytes]
```

Overhead per chunk: 40 bytes. Server's `max_chunk_size` includes overhead. Actual plaintext per chunk: `max_chunk_size - 40`.

## Key Distribution

Key embedded in share link URL fragment (never sent to server):

```
https://server/#id=SESSION_ID&encryption=xchacha20-poly1305&key=BASE64URL_KEY
```

Fragment is client-side only — server sees encrypted bytes, never the key.

## Flow

```
Sender: plaintext → encryptChunk(data, key) → [nonce|ciphertext|tag] → upload to server
Server: stores encrypted bytes in buffer, relays to receivers
Receiver: download → decryptChunk(data, key) → plaintext → save
```

## Key Encoding

`keyToBase64url()` / `base64urlToKey()` — RFC 4648 base64url, no padding.
