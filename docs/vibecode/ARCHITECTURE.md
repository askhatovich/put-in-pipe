# Server Architecture

C++20 server using Crow web framework, ASIO for async I/O. Captcha uses built-in Skaptcha (ChaCha20-based). Server does NOT perform any encryption — it relays encrypted chunks as opaque binary. E2E encryption (XChaCha20-Poly1305 via libsodium) happens only in clients (browser and Qt desktop).

## Source Layout

```
src/
  main.cpp                    # CLI parsing, config loading, Crow app setup
  webapi.h/cpp                # All HTTP routes + WebSocket handlers
  client.h/cpp                # Connected user: ID, name, WS, timeout timer
  clientlist.h/cpp            # Singleton: thread-safe client registry
  transfersession.h/cpp       # One file transfer: sender, receivers, buffer
  transfersessionlist.h/cpp   # Singleton: session registry with lifetime timer
  buffer.h/cpp                # Chunk queue with sanitization logic
  chunk.h/cpp                 # Single chunk: data + reference counting
  websocketconnection.h/cpp   # Crow WS wrapper + RAII helper
  serializableevent.h/cpp     # JSON serialization for all WS events
  timercallback.h/cpp         # ASIO steady_timer wrapper
  observerpattern.h           # Publisher/Subscriber template
  atomicset.h                 # Thread-safe set (expected consumers)
  config/config.h/cpp         # INI config singleton
  config/inireader.h/cpp      # INI parser
  captcha/skaptcha.h/cpp      # Captcha generation + validation
  captcha/token.h/cpp         # Captcha token management
  generated_index_html.h      # Auto-generated: embedded web UI
```

## Key Classes

### Client
- Internal ID (`m_id`, secret) + public ID (`m_publicId`, shared with others)
- `weak_ptr<WebSocketConnection>` — WS may disconnect and reconnect
- `m_wsTimeoutTimer` — fires after `clientTimeout` (60s), removes client
- `m_currentChunkIndex` — updated on each `confirm_chunk`
- Observer pattern: subscribes to session events, publishes online/offline/name changes

### ClientList (Singleton)
- `map<string, shared_ptr<Client>>` protected by mutex
- Own `asio::io_context` on dedicated thread
- `create()` / `get()` / `remove()` / `count()`

### TransferSession
- `weak_ptr<Client>` for sender, `list<weak_ptr<Client>>` for receivers
- Owns `Buffer` for chunk storage
- `m_receiversMutex` protects receiver list
- `m_initialFreezeTimer` — auto-drops freeze after config timeout
- Completion type: ok, timeout, senderIsGone, noReceivers

### TransferSessionList (Singleton)
- `map<string, SessionAndTimeout>` protected by mutex
- Own `asio::io_context` on dedicated thread
- Session lifetime timer (`max_lifetime`, default 2h)
- `create()` / `get()` / `remove()` / `count()`

### WebAPI
- Single Crow app with all routes
- Middleware: CookieParser, XForwardedFor (reverse proxy support)
- WebSocket: accept/connect/message/close handlers
- ETag caching for embedded HTML

## Observer Pattern

```
Publisher<EventType> → notifySubscribers(event, data)
Subscriber<EventType> → update(event, data)

Client publishes:  ClientsDirect (connected, disconnected, nameChanged)
                   ClientInternal (destroyed)

TransferSession publishes: TransferSession events (14 types)
                           TransferSessionForSender (newChunkIsAllowed)

TransferSession subscribes to: ClientInternal (detects client destruction)
Client subscribes to: ClientsDirect (other clients), TransferSession, TransferSessionForSender
```

## Threading Model

- Crow HTTP/WS: thread pool (ASIO)
- ClientList: own io_context + thread (timers)
- TransferSessionList: own io_context + thread (timers)
- Buffer/Chunk: mutex-protected, called from any thread
- TimerCallback: ASIO steady_timer on provided io_context

## Destructor Flow (Session Completion)

```
TransferSessionList::remove(id)
  → shared_ptr goes out of scope
  → TransferSession::~TransferSession()
    1. notifySubscribers(complete, status)  — sends "complete" event to all
    2. Collect all client IDs
    3. Schedule 1-second ASIO timer
    4. Timer fires → ClientList::remove(id) for each
       → Client::~Client() → WebSocketConnection::close()
```

The 1-second delay ensures the "complete" text frame is delivered before the WS close frame.
