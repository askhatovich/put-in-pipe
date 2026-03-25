# Build & Configuration

## Server Build

```bash
# 1. Build web frontend
cd web && npm install && npm run build && cd ..

# 2. Embed HTML into C++ header
bash web/embed.sh

# 3. CMake build
cmake -B build -S src
cmake --build build

# 4. Run
./build/put-in-pipe --generate-config /tmp/config.ini
./build/put-in-pipe -c /tmp/config.ini
```

## CMake (src/CMakeLists.txt)

- C++20, Crow (header-only, bundled in src/crowlib/), ASIO, plog
- `APP_VERSION` from `$ENV{APP_VERSION}` or `git rev-parse --short HEAD`
- Core static library (`put-in-pipe-core`) + main executable
- Tests via GoogleTest (optional, `BUILD_TESTS=ON`)

## Web Frontend Build

- Vite + Svelte 5
- `vite-plugin-singlefile` — bundles everything into one HTML
- Output: `web/dist/index.html`

## Embedding (web/embed.sh)

Converts `web/dist/index.html` to C++ header `src/generated_index_html.h`:
- Reads binary file
- Outputs hex array: `static const unsigned char index_html_data[] = { 0x3C, ... };`
- Generates `getIndexHtml()` function returning `std::string`

Server serves this at `GET /` with ETag caching and `Cache-Control: no-cache`.

## Configuration (INI)

Default path: `/etc/put-in-pipe/config.ini`. Override with `-c <path>`.

```ini
[server]
log_level = info              # info|verbose|debug|warning|error|fatal|none
bind_address = 0.0.0.0
bind_port = 2233

[client]
max_count = 500               # Max concurrent clients
without_captcha_threshold = 500  # Clients before captcha required
captcha_lifetime = 180         # Captcha validity (seconds)
timeout = 60                   # WS disconnect grace period (seconds)

[session]
count_limit = 100              # Max concurrent sessions
max_chunk_size = 5242880       # 5 MB per chunk
chunk_queue_max_size = 10      # Buffer queue depth
max_lifetime = 7200            # Session timeout (seconds, 2 hours)
max_consumer_count = 5         # Max receivers per session
max_initial_freeze_duration = 120  # Freeze window (seconds)
```

## Memory Budget

```
max_sessions × max_chunk_size × chunk_queue_max_size
= 100 × 5MB × 10 = 5GB worst case
```

Server logs warning if this exceeds system RAM.

## CLI

```
put-in-pipe [options]
  -h, --help                Show help
  -v, --version             Show version
  -c, --config <path>       Config file path
  -g, --generate-config <path>  Generate default config
```

## CI/CD (.github/workflows/release.yml)

Triggered on tag `v*`. Builds:
- Debian packages (multiple distros)
- Static Alpine binary
- RPM packages
- Creates GitHub Release with artifacts

Steps: build web → embed → cmake → package.
