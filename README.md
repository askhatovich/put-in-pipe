*[Читать на русском](README_ru.md)*

# Put-In-Pipe

A privacy-preserving streaming file transfer server. The server acts as a relay between a sender and multiple receivers, transferring files in chunks without storing the complete file on disk. Clients never connect directly to each other -- no IP addresses are disclosed between participants.

## Features

- **Server as relay** -- clients communicate only through the server, never directly
- **Streaming chunk-based transfer** -- no file size limit tied to server disk or memory
- **Multiple receivers** per session (configurable limit)
- **Captcha protection** against automated abuse
- **E2E encryption** -- XChaCha20-Poly1305 via libsodium; the key lives in the URL anchor (`#`), which is never sent to the server
- **REST API + WebSocket** for real-time transfer events
- **Built-in web UI** -- Svelte 5 SPA bundled into a single HTML file and embedded into the server binary
- **Configurable** via INI file

## Building

### Backend only

Requires a C++20 compiler and CMake 3.16+. If the frontend has not been built yet, you need to build it first (see below).

```sh
cmake -B build src
cmake --build build
```

### Full build (frontend + backend)

Requires Node.js 18+ and npm in addition to the C++ toolchain.

```sh
# 1. Build the frontend
cd web
npm install
npm run build    # produces web/dist/index.html (single-file SPA)
./embed.sh       # generates src/generated_index_html.h
cd ..

# 2. Build the backend (with embedded frontend)
cmake -B build src
cmake --build build
```

After this, `GET /` serves the complete web application — no separate static file server needed.

### Frontend development

The frontend lives in `web/` and is a standard Vite + Svelte 5 project.

```sh
cd web
npm install
npm run dev      # starts Vite dev server with hot reload (http://localhost:5173)
npm run build    # production build → web/dist/index.html
```

During development, run `npm run dev` and configure your browser to proxy API calls to the running backend (`http://localhost:2233`), or run the backend simultaneously and open the Vite dev server URL.

**Tech stack:**
- [Svelte 5](https://svelte.dev/) -- reactive UI framework
- [Vite](https://vite.dev/) + [vite-plugin-singlefile](https://github.com/nickreese/vite-plugin-singlefile) -- bundler, inlines all assets into one HTML
- [libsodium-wrappers-sumo](https://github.com/nickvergessen/libsodium.js) -- XChaCha20-Poly1305 encryption in the browser

**Project structure:**
```
web/
  src/
    App.svelte              -- root component, app state machine
    lib/
      api.js                -- REST API client
      ws.js                 -- WebSocket client with event dispatch
      crypto.js             -- libsodium encrypt/decrypt wrapper
      i18n.js               -- en/ru translations, auto-detect
      url.js                -- URL anchor (#) parsing and building
    components/
      EntryScreen.svelte    -- initial screen (send/receive choice)
      Captcha.svelte        -- captcha display and input
      SessionSender.svelte  -- sender session view
      SessionReceiver.svelte-- receiver session view
      TransferComplete.svelte-- transfer result screen
      MemberList.svelte     -- session participants panel
      ProgressPanel.svelte  -- file progress and buffer status
      Footer.svelte         -- server stats + copyright
```

### Embedding the frontend

The `web/embed.sh` script converts the built `web/dist/index.html` into a C++ header (`src/generated_index_html.h`) containing the HTML as a byte array. The backend includes this header and serves it at `/`.

This file is auto-generated and listed in `.gitignore` -- it must be regenerated after any frontend change:

```sh
cd web && npm run build && ./embed.sh
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
