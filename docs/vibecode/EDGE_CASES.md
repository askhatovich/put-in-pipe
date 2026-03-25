# Edge Cases & Solutions

## 1. WS Close Frame Races "complete" Event

**Problem:** Session destructor sends `complete` via sendText() then destroys clients (which close WS). Close frame may arrive before text frame.

**Solution:** 1-second ASIO timer delay between sending `complete` and removing clients from ClientList. Clients stay alive long enough for text frame delivery.

## 2. Expired weak_ptr in Receiver List

**Problem:** `/api/me/leave` destroys client immediately. When `removeReceiver()` runs (from Client destructor notification), the `weak_ptr` is already expired. `iter->lock()` returns nullptr, entry not erased, `m_dataReceivers.empty()` stays false.

**Solution:** `removeReceiver()` erases expired weak_ptrs during iteration. Cleans up stale entries so `empty()` correctly reflects actual state.

## 3. No Receivers After Freeze Drops

**Problem:** All receivers leave before freeze drops. After drop, session has no receivers and no one to receive data, but termination condition `someChunksWasRemoved` may be false.

**Solution:** `removeReceiver()` checks `noReceiversLeft && (someChunksWasRemoved || !initialChunksFreezing)`. If freeze is already dropped, session terminates regardless of chunk removal state.

## 4. Sender Disconnects After EOF, No Receivers

**Problem:** Sender uploaded everything and disconnected. If `eof()` is true, the old code returned early (ignoring sender departure). Session hung forever if no receivers.

**Solution:** In `update(ClientInternal::destroyed)`, if sender is gone AND `eof()` AND `m_dataReceivers.empty()` → terminate with `noReceivers`.

## 5. Receiver Completes But Server Doesn't Know

**Problem:** Web receiver calls `disconnect()` after completing download. Server keeps client for 60s timeout. Sender waits.

**Solution:** `handleComplete()` in App.svelte calls `leave()` (fire-and-forget) after `disconnect()` for receivers. Server immediately removes client → session terminates.

## 6. name_changed Not Echoed to Self

**Problem:** Server's pub/sub sends `name_changed` only to OTHER subscribers, not the client who changed their name.

**Solution:** Client updates own name locally after sending `new_name` action. Desktop app: updates `m_senderName` or receiver entry directly. Web: updates via reactive state.

## 7. File System Access API Not Available

**Problem:** Firefox/Safari don't support `showSaveFilePicker`. Large files consume all RAM.

**Solution:** Feature detection (`typeof window.showSaveFilePicker === 'function'`). Fallback: in-memory Map. Warning shown with link to desktop app.

## 8. Out-of-Order Chunk Downloads (Parallel)

**Problem:** With 4 parallel downloads, chunks arrive out of order. Sequential file write requires ordered data.

**Solution:** `flushChunkToDisk()` writes chunk if `index == nextWriteIndex`, otherwise buffers in `chunkBuffer` Map. After writing, drains buffer for consecutive indices. Max ~4 chunks in memory (20MB).

## 9. Buffer Full (Sender Flow Control)

**Problem:** Sender uploads faster than receivers download.

**Solution:** Server limits buffer to `chunk_queue_max_size` (10). When full, `addChunk()` fails. After sanitize frees space, `newChunkIsAllowed(true)` event sent to sender.

## 10. Receiver Joins After Chunks Deleted

**Problem:** After freeze drops and some chunks are removed, new receiver would get incomplete file.

**Solution:** `addNewToExpectedConsumers()` fails if `!initialChunksFreezing && someChunkWasRemoved`. Join endpoint returns error "It is impossible to join the session".

## 11. currentChunkIndex Always Zero

**Problem:** `Client::m_currentChunkIndex` was declared but never updated. New receivers saw `#0` for existing receivers.

**Solution:** `setCurrentChunkIndex(chunkId)` called in WebSocket handler on each `confirm_chunk` action. Sent in `start_init` to new participants.

## 12. writable.close() Blocks UI

**Problem:** File System Access API `close()` can take seconds for large files (fsync). User sees frozen UI.

**Solution:** Show "Finalizing file" overlay with spinner during `close()`. `beforeunload` handler prevents accidental tab close. UI transitions to complete screen only after close finishes.

## 13. Mobile Chromium Blob Download

**Problem:** Android Chromium loses blob URL reference on page navigation/reload.

**Solution:** Save blob to IndexedDB before reload. On reload, check IndexedDB for pending download (60s TTL). Detected via user agent check (Android + not Firefox).
