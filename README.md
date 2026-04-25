*[Читать на русском](README_ru.md)*

# Put-In-Pipe

A privacy-preserving streaming file transfer server. The server acts as a relay between a sender and one or more receivers, transferring files in chunks without persisting the complete file to disk. Clients do not establish direct connections to each other; participant IP addresses are not disclosed to other parties.

## Features

- **Relay-based transport** -- clients communicate exclusively through the server, with no direct peer-to-peer connections.
- **Streaming chunk-based transfer** -- file size is not bounded by server disk or memory capacity.
- **Multiple receivers** per session, with a configurable upper limit.
- **Captcha protection** against automated abuse.
- **End-to-end encryption** -- XChaCha20-Poly1305 via libsodium; the encryption key is transmitted in the URL fragment (`#`), which is not sent to the server.
- **REST API and WebSocket interface** for real-time transfer events.
- **Built-in web interface** -- a Svelte 5 single-page application, bundled into a single HTML file and embedded in the server binary.
- **Configuration** via an INI file.

## Building

### Backend only

Requires a C++20 compiler and CMake 3.16 or later. The frontend must be built before the backend (see below).

```sh
cmake -B build src
cmake --build build
```

### Full build (frontend and backend)

Requires Node.js 18 or later and npm, in addition to the C++ toolchain.

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

After this build, `GET /` serves the complete web application; a separate static file server is not required.

### Frontend development

The frontend resides in `web/` and is a standard Vite + Svelte 5 project.

```sh
cd web
npm install
npm run dev      # starts Vite dev server with hot reload (http://localhost:5173)
npm run build    # production build → web/dist/index.html
```

During development, run `npm run dev` and configure the browser to proxy API requests to the running backend (`http://localhost:2233`), or run the backend in parallel and open the Vite development server URL.

**Technology stack:**
- [Svelte 5](https://svelte.dev/) -- reactive UI framework.
- [Vite](https://vite.dev/) with [vite-plugin-singlefile](https://github.com/nickreese/vite-plugin-singlefile) -- bundler that embeds all assets into a single HTML file.
- [libsodium-wrappers-sumo](https://github.com/nickvergessen/libsodium.js) -- XChaCha20-Poly1305 encryption in the browser.

**Project structure:**
```
web/
  src/
    App.svelte              -- root component and application state machine
    lib/
      api.js                -- REST API client
      ws.js                 -- WebSocket client with event dispatching
      crypto.js             -- libsodium encryption and decryption wrapper
      i18n.js               -- English and Russian translations with automatic detection
      url.js                -- URL fragment (#) parsing and construction
    components/
      EntryScreen.svelte    -- initial screen (send or receive selection)
      Captcha.svelte        -- captcha display and input
      SessionSender.svelte  -- sender session interface
      SessionReceiver.svelte-- receiver session interface
      TransferComplete.svelte-- transfer result screen
      MemberList.svelte     -- session participants panel
      ProgressPanel.svelte  -- file progress and buffer status
      Footer.svelte         -- server statistics and copyright notice
```

### Embedding the frontend

The `web/embed.sh` script converts the built `web/dist/index.html` into a C++ header (`src/generated_index_html.h`) that contains the HTML as a byte array. The backend includes this header and serves its contents at `/`.

The header is generated automatically and is listed in `.gitignore`; it must be regenerated after any frontend change:

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

The complete list of available options is provided in [`contrib/config.ini.example`](contrib/config.ini.example). The principal sections are:

- **`[server]`** -- bind address and port.
- **`[client]`** -- maximum number of clients, captcha threshold, captcha lifetime, and client timeout.
- **`[session]`** -- session limit, maximum chunk size, buffer queue size, session lifetime, maximum number of receivers, and initial freeze duration.

## Testing

The test suite is based on Google Test and is built by default.

```sh
cd build && ctest --output-on-failure
```

## API documentation

The API is documented in [docs/web_interface_api.md](docs/web_interface_api.md).

## License

[GPL-3.0-or-later](LICENSE)

Author: Roman Lyubimov
