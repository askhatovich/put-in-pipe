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
- **Freeze drops by:** sender `drop_freeze` action OR server auto-timer (120s)
- **After freeze + chunks removed:** new receivers CANNOT join (would miss data)

```
Freeze active:   [upload chunks] [receivers download] [chunks retained]
Freeze drops:    [confirmed chunks deleted] [new chunks flow through]
```

## Completion Paths

### 1. Normal (ok)
All chunks confirmed by all receivers + EOF set → `chunkCount == 0 && eof()` → session removed.

### 2. Manual Terminate (sender_is_gone)
Sender sends `terminate_session` → immediate removal.

### 3. Sender Disconnect (sender_is_gone)
Sender WS closes → 60s timeout → client destroyed → if `!eof()`: terminate with sender_is_gone. If `eof()` and no receivers: terminate with no_receivers.

### 4. No Receivers (no_receivers)
All receivers removed AND (`someChunksWasRemoved` OR `!initialChunksFreezing`) → terminate.

Triggered in:
- `removeReceiver()` — when last receiver leaves after freeze dropped
- `dropInitialChunksFreeze()` — when freeze drops with no receivers

### 5. Timeout (timeout)
Session lifetime exceeds `max_lifetime` (default 2h) → `setTimedout()` → remove.

## Destructor Flow

```
TransferSession::~TransferSession()
  1. Broadcast "complete" event to all subscribers
  2. Collect sender + receiver client IDs
  3. Schedule 1-second ASIO timer
  4. Timer fires → ClientList::remove(id) for each
     → Client::~Client() → WS close
```

1-second delay ensures "complete" text frame arrives before WS close frame.

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
