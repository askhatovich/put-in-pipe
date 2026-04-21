# HTTP + WebSocket Protocol

## HTTP Endpoints

| Method | Path | Auth | Purpose |
|--------|------|------|---------|
| GET | `/` | No | Serve embedded web UI (ETag cached) |
| GET | `/api/statistics/current` | No | `{current_user_count, current_session_count, max_user_count, max_session_count, version}` |
| GET | `/api/identity/request?name=<n>` | No | Auth. 201=ok, 401=captcha, 503=full |
| POST | `/api/identity/confirmation` | No | Captcha answer. Body: `{captcha_answer, client_id, captcha_token, name}` |
| GET | `/api/me/info` | Cookie | `{id (publicId), name, session}` |
| POST | `/api/me/leave` | Cookie | Remove client immediately |
| POST | `/api/session/create` | Cookie | Create session. Optional JSON body: `{auto_drop_freeze: bool}`. 201: `{id}` |
| GET | `/api/session/join?id=<id>` | Cookie | Join session. 202: `{id}` |
| POST | `/api/session/chunk` | Cookie | Upload chunk (binary body). 202 or 421 (buffer full) |
| GET | `/api/session/chunk?id=<idx>` | Cookie | Download chunk. 200 (binary) or 404 |
| WS | `/api/ws` | Cookie | WebSocket for real-time events |

## Authentication

Cookie `putin=<internal_id>` set as HttpOnly on identity request/confirmation. All session endpoints require this cookie.

Captcha required when `clientCount >= without_captcha_threshold`. Response includes `captcha_image` (base64 PNG), `captcha_token`, `captcha_answer_length`, `client_id`.

## WebSocket

### Connection

On WS accept: validate cookie + client has joined session. One WS per client (reconnect drops previous).

On connect: server sends `start_init` with full session state:
```json
{
  "event": "start_init",
  "data": {
    "session_id": "...",
    "limits": { "max_chunk_size", "max_chunk_queue", "max_receiver_count", "max_initial_freeze" },
    "members": {
      "sender": { "id", "name", "is_online" },
      "receivers": [{ "id", "name", "is_online", "current_chunk" }]
    },
    "state": {
      "current_chunk", "upload_finished", "initial_freeze", "initial_freeze_remaining",
      "some_chunk_was_removed", "expiration_in",
      "file": { "name", "size" },
      "chunks": [{ "index", "size" }]
    },
    "transferred": { "global": { "from_sender", "to_receivers" }, "received_by_you" }
  }
}
```

### Client → Server Actions

Format: `{"action": "name", "data": {...}}`

| Action | Role | Data | Description |
|--------|------|------|-------------|
| `set_file_info` | Sender | `{name, size}` | Set file metadata (name sanitized, max 255 chars) |
| `upload_finished` | Sender | `{}` | Mark upload complete (EOF) |
| `drop_freeze` | Sender | `{}` | Drop initial freeze |
| `terminate_session` | Sender | `{}` | Force terminate |
| `kick_receiver` | Sender | `{id}` | Remove receiver |
| `new_name` | Any | `{name}` | Change name (truncated to 20 chars). NOT echoed to self |
| `confirm_chunk` | Any | `{index}` | Confirm chunk received. Updates currentChunkIndex |
| `get_chunk` | Any | `{index}` | Request chunk (returns binary or error event) |
| `ack` | Any | `{id}` | Acknowledge a server event that carried an `id` field |
| Binary frame | Sender | raw bytes | Upload chunk |

### Server → Client Events

Format: `{"event": "name", "data": {...}}`

| Event | Data | Description |
|-------|------|-------------|
| `start_init` | (see above) | Full state on WS connect |
| `online` | `{id, status}` | Member online/offline |
| `name_changed` | `{id, name}` | Member renamed. NOT sent to the client who changed it |
| `new_receiver` | `{id, name}` | Receiver joined |
| `receiver_removed` | `{id}` | Receiver left/kicked. NOT sent to kicked receiver |
| `file_info` | `{name, size}` | File metadata set |
| `new_chunk` | `{index, size}` | Chunk available |
| `chunk_download` | `{id, index, action}` | `action`: "started" or "finished" |
| `chunk_removed` | `{id: [indices]}` | Chunks sanitized |
| `bytes_count` | `{value, direction}` | `direction`: "from_sender" or "to_receivers" |
| `personal_received` | `{bytes}` | Receiver's personal byte count |
| `chunks_unfrozen` | `{}` | Freeze dropped |
| `upload_finished` | `{}` | Sender EOF |
| `complete` | `{status}` | Session ended: "ok", "timeout", "sender_is_gone", "no_receivers". **ACK-required** |
| `kicked` | `{}` | Sent to a receiver that the sender just kicked. **ACK-required** |
| `new_chunk_allowed` | `{status}` | Sender only: buffer has space (flow control) |
| Error events | varies | `set_file_info_failure`, `add_chunk_failure`, `requested_chunk_not_found`, `unknown_action` |

## Event Acknowledgment

Terminal events that precede a WS close carry a top-level `id` (uint64) field. The client MUST reply with `{"action": "ack", "data": {"id": <same id>}}`. The server closes the WS only after the ACK arrives, or after a fallback timer (default 2 s). This replaces the previous 1-second arbitrary delay that was used to avoid racing the close frame against the final text frame.

- Events currently marked ACK-required: `complete`, `kicked`.
- Other events remain fire-and-forget (no `id` field).
- A client that ignores the ACK requirement will still see the close — it just happens 2 s later via the fallback.

Example:

```json
// server → client
{"event": "complete", "id": 17, "data": {"status": "ok"}}

// client → server
{"action": "ack", "data": {"id": 17}}
```

## Important Protocol Behaviors

1. **name_changed NOT echoed to self** — client must update locally
2. **kicked IS sent to the removed receiver** (ACK-required); the receiver is removed from `m_dataReceivers` before the event is sent, so they do not receive a `receiver_removed` for themselves
3. **confirm_chunk updates currentChunkIndex** — visible to other participants in start_init and chunk_download events
4. **One WS per client** — reconnecting drops previous connection
5. **WS disconnect ≠ client removal** — client stays for `clientTimeout` (60s) allowing reconnect
6. **POST /api/me/leave = immediate removal** — no timeout wait
7. **Terminal events are delivery-confirmed** — `complete` and `kicked` wait for ACK before the server tears down the WS
