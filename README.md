*[Читать на русском](README_ru.md)*

# Put-In-Pipe

A privacy-preserving streaming file transfer server. The server acts as a relay between a sender and multiple receivers, transferring files in chunks without storing the complete file on disk. Clients never connect directly to each other -- no IP addresses are disclosed between participants.

## Features

- **Server as relay** -- clients communicate only through the server, never directly
- **Streaming chunk-based transfer** -- no file size limit tied to server disk or memory
- **Multiple receivers** per session (configurable limit)
- **Captcha protection** against automated abuse
- **Room for E2E encryption** on the frontend side (e.g. ChaCha20-Poly1305 with the key in the URL anchor, invisible to the server)
- **REST API + WebSocket** for real-time transfer events
- **Configurable** via INI file

## Building

Requires a C++20 compiler and CMake 3.16+.

```sh
cmake -B build src
cmake --build build
```

## Running

```sh
# Show help
./build/put-in-pipe --help

# Generate a default configuration file
./build/put-in-pipe --generate-config ./config.ini

# Start the server
./build/put-in-pipe --config ./config.ini
```

### CLI options

| Option | Description |
|---|---|
| `-c, --config <path>` | Path to config file (default: `/etc/put-in-pipe/config.ini`) |
| `-g, --generate-config <path>` | Generate a default config at the given path |
| `-h, --help` | Show help |

## Configuration

See [`contrib/config.ini.example`](contrib/config.ini.example) for all available options. Key sections:

- **`[server]`** -- bind address and port
- **`[client]`** -- max clients, captcha threshold, captcha lifetime, client timeout
- **`[session]`** -- session limit, max chunk size, buffer queue size, session lifetime, max receivers, initial freeze duration

## Testing

Tests use Google Test and are built by default.

```sh
cd build && ctest --output-on-failure
```

## API Documentation

See [docs/web_interface_api.md](docs/web_interface_api.md).

## License

[GPL-3.0-or-later](LICENSE)

Author: Roman Lyubimov
