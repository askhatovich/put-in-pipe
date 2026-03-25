# Web Client Architecture

Svelte 5 SPA, built with Vite, embedded into server binary as single HTML file.

## Build Pipeline

```
web/src/ → Vite build → web/dist/index.html (single file via vite-plugin-singlefile)
         → web/embed.sh → src/generated_index_html.h (C++ hex array)
         → CMake links into server binary
         → Served at GET / with ETag caching
```

## Component Tree

```
App.svelte (state machine)
  ├── NameBadge (header: name edit, server stats)
  ├── EntryScreen (file drop-zone + join link input)
  ├── Captcha (captcha image + answer input)
  ├── SessionSender (upload: progress, member list, share link, QR)
  │     ├── MemberList (sender + receivers with status)
  │     ├── ProgressPanel (progress bar, bytes, buffer)
  │     └── SessionTimer (expiration countdown)
  ├── SessionReceiver (download: progress, member list, leave button)
  │     ├── MemberList
  │     ├── ProgressPanel
  │     └── SessionTimer
  ├── TransferComplete (status icon, save button, restart)
  └── Footer (stats, version, language toggle)
```

## Screen States

`loading` → `entry` → `captcha`(optional) → `connecting` → `sender`/`receiver` → `complete`

## Libraries (web/src/lib/)

| File | Purpose |
|------|---------|
| `api.js` | HTTP client for all REST endpoints |
| `ws.js` | WebSocket: connect, disconnect, send, on/off events, auto-reconnect |
| `crypto.js` | libsodium XChaCha20-Poly1305: encrypt/decrypt chunks, key gen/encode |
| `url.js` | Share link: buildAnchor(), parseAnchor() |
| `i18n.js` | EN/RU translations, funny name generator, language detection |

## Sender Flow (SessionSender.svelte)

1. File selected on entry screen → `handleFileSelected(file)` in App.svelte
2. Auth + session create + WS connect
3. `set_file_info` action sent
4. Upload loop: `file.slice()` → encrypt → WS binary send
5. Flow control: wait for `new_chunk` ack, pause if buffer full
6. Track per-receiver progress via `chunk_download finished` events
7. Receiver `done` flag: `receiverChunksDone[id] >= highestChunkIndex && uploadFinished`
8. Session completes via server's `complete` event

## Receiver Flow (SessionReceiver.svelte)

1. Join via link → auth + session join + WS connect
2. Receive `start_init` with existing chunks and file info
3. Download loop: HTTP GET each chunk → decrypt → confirm_chunk
4. Track `chunksConfirmed` and `chunksAcknowledged` (from `chunk_download finished`)
5. Completion: `uploadFinished && noMorePendingChunks && chunksAcknowledged >= chunksConfirmed`
6. On complete: `leave()` called to immediately remove from server (prevents 60s timeout)

## File System Access API (Chrome/Edge only)

For large files, writes directly to disk instead of accumulating in RAM:

```
1. On file_info: showSaveFilePicker() → user picks save location
2. writable = await fileHandle.createWritable()
3. Each chunk: flushChunkToDisk(index, data)
   - Sequential: write if index == nextWriteIndex, drain buffer
   - Out-of-order: buffer in chunkBuffer Map until gap filled
4. On complete: writable.close() with finalizing overlay
5. beforeunload handler prevents accidental tab close during finalize
```

**Fallback (Firefox/Safari):** chunks stored in `Map<index, Uint8Array>` in memory. Warning shown with link to desktop app.

**writableReady promise:** `fetchAndDecryptChunk()` awaits this before downloading, ensuring save dialog completes before any chunks arrive.

## MemberList Features

- Sender: green/gray dot (online/offline)
- Receivers: green checkmark (done) or green/gray dot + `#N` chunk index
- Own name highlighted in green (`#7dcea0`)
- Kick button (sender only, × icon)

## i18n

Two languages: EN, RU. Auto-detected from `navigator.language`. Toggle in footer. ~35 translation keys.

Funny names: 50 per language, randomly assigned on first visit, persisted in localStorage.

## Entry Screen

Drop-zone for file selection (click or drag-and-drop) + join link input with paste support. No separate "file select" screen — file picker is inline on the main page.
