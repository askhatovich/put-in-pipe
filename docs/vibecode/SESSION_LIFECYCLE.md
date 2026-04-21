# Session Lifecycle

## Creation

```
1. Client authenticates (identity/request + optional captcha)
2. POST /api/session/create → TransferSession created, sender joins
3. Session gets unique ID, freeze timer starts (120s default)
4. Sender connects WS → receives start_init
```

## Active Transfer

```
5. Sender: set_file_info → file_info event to all
6. Sender: upload chunks (binary WS or POST) → new_chunk events
7. Receivers: GET /api/session/join → WS connect → start_init
8. Receivers: download chunks (HTTP GET) → confirm_chunk → chunk_download events
9. Flow control: sender pauses if buffer full, resumes on new_chunk_allowed
```

## Freeze Mechanism

Initial freeze prevents chunk deletion while waiting for receivers.

- **During freeze:** chunks NOT removed after confirmation (sanitize() returns early)
- **After freeze drops:** confirmed chunks removed, freeing buffer space
- **Freeze drops by:**
  - sender `drop_freeze` action, OR
  - server auto-timer (`max_initial_freeze_duration`, default 120 s), OR
  - auto-drop on first receiver confirmation when the session was created with `auto_drop_freeze: true`
- **After freeze + chunks removed:** new receivers CANNOT join (would miss data)

```
Freeze active:   [upload chunks] [receivers download] [chunks retained]
Freeze drops:    [confirmed chunks deleted] [new chunks flow through]
```

## Completion Paths

### 1. Normal (ok)
All chunks confirmed by all receivers + EOF set → session removed. Triggered from either:
- `setChunkAsReceived()` after the last confirmation if EOF is already set;
- `setEndOfFile()` if all chunks were already confirmed before EOF arrived (handles the race where the receiver finishes download before `upload_finished` reaches the server);
- `removeReceiver()` if the last receiver leaves after the full file was already transferred — this is reclassified from `no_receivers` to `ok`.
- `removeReceiver()` in auto-drop mode (`auto_drop_freeze: true`) — any time the last receiver leaves, even mid-transfer.

### 2. Manual Terminate (sender_is_gone)
Sender sends `terminate_session` → immediate removal.

### 3. Sender Disconnect (sender_is_gone)
Sender WS closes → 60s timeout → client destroyed → if `!eof()`: terminate with sender_is_gone. If `eof()` and no receivers: terminate with no_receivers.

### 4. No Receivers (no_receivers)
All receivers removed AND (`someChunksWasRemoved` OR `!initialChunksFreezing`) AND transfer wasn't fully delivered → terminate.

Triggered in:
- `removeReceiver()` — when last receiver leaves after freeze dropped
- `dropInitialChunksFreeze()` — when freeze drops with no receivers

**Exception (auto-drop mode):** when the session was created with `auto_drop_freeze: true`, the last receiver leaving always terminates the session with `ok` — regardless of whether the transfer finished, whether any chunk was removed, or whether the freeze was dropped. The sender explicitly opted into fire-and-forget: whatever was delivered to the (now departed) receivers is treated as a successful outcome.

### 5. Timeout (timeout)
Session lifetime exceeds `max_lifetime` (default 2h) → `setTimedout()` → remove.

## Destructor Flow

```
TransferSession::~TransferSession()
  1. Publisher::notifySubscribers(complete, completeType)
     → each Client::update() calls sendTextWithAck("complete", onAckCallback)
     → server injects "id":N, sends JSON, arms fallback timer (2 s)
  2. Browser receives event, auto-ACKs via {"action":"ack","data":{"id":N}}
  3. Server matches ACK → fires onAckCallback → ClientList::remove(id)
     → Client::~Client() → WS close
  4. If ACK never arrives within 2 s, the fallback timer fires
     the same onAckCallback (fail-open).
```

There is no fixed delay — each client closes as soon as it has confirmed the `complete` event. The fallback exists only for dead clients.

## removeReceiver() Flow (Critical)

```
1. Lock receiver mutex
2. Iterate m_dataReceivers:
   - If weak_ptr locks → check publicId match → erase if found
   - If weak_ptr expired → erase (cleanup stale references)
3. noReceiversLeft = m_dataReceivers.empty()
4. Unlock

5. removeOneFromExpectedConsumers(publicId) → sanitize buffer
6. Destroy removed client (ClientList::remove)
7. Broadcast receiver_removed event

8. If noReceiversLeft AND (someChunksWasRemoved OR !initialChunksFreezing):
   → Terminate with no_receivers
```

**Key fix:** expired weak_ptrs are cleaned up during iteration. Without this, `/api/me/leave` (which destroys the client before removeReceiver runs) would leave stale entries, preventing `m_dataReceivers.empty()` from being true.
