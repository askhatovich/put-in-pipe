# Put-In-Pipe — Vibecode Documentation

Context documents for AI agents and developers. Keep up to date when making changes.

| File | Contents |
|------|----------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | Server classes, ownership, threading, observer pattern |
| [PROTOCOL.md](PROTOCOL.md) | Full HTTP + WebSocket protocol, all events and actions |
| [SESSION_LIFECYCLE.md](SESSION_LIFECYCLE.md) | Creation, transfer, freeze, completion, all termination paths |
| [BUFFER_AND_CHUNKS.md](BUFFER_AND_CHUNKS.md) | Chunk management, sanitization, expected consumers, flow control |
| [WEB_CLIENT.md](WEB_CLIENT.md) | Svelte 5 architecture, components, File System Access API, i18n |
| [ENCRYPTION.md](ENCRYPTION.md) | XChaCha20-Poly1305, key distribution, chunk wire format |
| [BUILD_AND_CONFIG.md](BUILD_AND_CONFIG.md) | CMake, Vite, embed.sh, INI config, CI/CD |
| [EDGE_CASES.md](EDGE_CASES.md) | Known issues, race conditions, solutions |

## Build

```bash
# Server
cd web && npm run build && cd ..
bash web/embed.sh
cmake -B build -S src && cmake --build build
./build/put-in-pipe --generate-config /tmp/config.ini
./build/put-in-pipe -c /tmp/config.ini

# Desktop client (separate repo)
# https://github.com/askhatovich/putinqa
```
