Karing
=====

Karing is a lightweight pastebin-like API service built on Drogon. It stores short text or small files (images/audio) in SQLite and exposes simple HTTP endpoints for create/search/retrieve/update/delete. It runs as a single binary, supports API keys, IP allow/deny control, reverse proxies, and Docker.

- Fast C++17 server (Drogon)
- SQLite persistence (single file)
- Text and file blobs (images/audio)
- Full‑text search (FTS5) over text and filenames
- API key auth + IP allow/deny
- Base path for reverse proxy subpaths
- TLS via reverse proxy or app flags

For a Japanese README, see `README-ja.md`.

Quick Start
-----------

- Prebuilt binaries
  - GitHub Actions upload Linux/macOS artifacts on pushes and tags. See the latest Releases for downloadable binaries.
- Docker (recommended for quick trials)
  - Image: `ghcr.io/recelsus/karing:latest`
  - Example:
    ```bash
    docker run --rm -p 8080:8080 \
      -e KARING_BASE_PATH=/myapp \
      -e KARING_LIMIT=100 \
      -v karing-data:/var/lib/karing \
      ghcr.io/recelsus/karing:latest
    ```
- Build from source (classic flow)
  ```bash
  mkdir build && cd build
  cmake .. -DCMAKE_BUILD_TYPE=Release
  make -j
  ```

Configuration
-------------

- Config file format: Drogon‑compatible JSON (named `karing.json`)
- Discovery order (later ones are fallbacks):
  - `--config <path>` or `KARING_CONFIG`
  - `$XDG_CONFIG_HOME/karing/karing.json`
  - `~/.config/karing/karing.json`
  - `/etc/karing/karing.json` (POSIX)
  - `config/karing.json` (bundled default)
- Windows paths
  - Config: `%APPDATA%\karing\karing.json`
  - DB default: `%LOCALAPPDATA%\karing\karing.db`
- Environment variables override config (e.g., `KARING_BASE_PATH`, `KARING_LIMIT`, etc.)
- Details: see `docs/config.md`.

Endpoints
----------------------

- `GET /` — raw latest (text/plain or inline file). With `id=`, returns that item inline. Add `json=true` to return JSON instead.
- `POST /` — create text (JSON `{ content }`) or file (multipart); returns `{ success: true, message: "Created", id }` with 201.
- `PUT /?id=` — replace
- `PATCH /?id=` — partial update
- `DELETE /?id=` — logical delete
- `GET /health` — service/build/limits/TLS/base_path
- `GET|POST /search` — list/search API (JSON only)
  - No params: latest items up to `limit` (default: runtime limit)
  - `limit`
  - `q` — FTS5 query (text: content, file: filename)
  - `type` — `text` | `file` (optional filter)
  - Response: `{ success: true, message: "OK", data: [...], meta: { count, limit, total? } }`
  - Auth: `GET /search` is allowed for read; `POST /search` is also treated as a read operation (no write role required).

Notes
- Base path support: the same endpoints are available under `<base_path>`, e.g., `/myapp`, `/myapp/health`, `/myapp/restore`, `/myapp/search`.
- Auth: API key via `X-API-Key` or `?api_key=`, role‑based (read/write). See `docs/config.md`.

Response format
---------------

- Success: `{ success: true, message: "OK" | "Created", ... }`
- Error: `{ success: false, code, message, details? }`

Search and FTS
--------------

- Uses SQLite FTS5 virtual table `karing_fts` with columns: `content` (text), `filename` (file).
- `/search` accepts GET query or POST JSON with the same fields.
- Disable FTS via `KARING_DISABLE_FTS=1` (then `/search` with `q` returns 503). Latest listing without `q` still works.
- Prefix matching: you can enable FTS prefix indexing by setting `KARING_FTS_PREFIX`, e.g. `KARING_FTS_PREFIX="2 3"` to index 2‑ and 3‑character prefixes. This improves leading‑substring matches like `hel*`.

Install (make install)
----------------------

- Standard install is supported via CMake:
  ```bash
  mkdir build && cd build
  cmake .. -DCMAKE_BUILD_TYPE=Release
  make -j
  sudo make install
  ```
- Installs binary to `${prefix}/bin/karing` and default config to `${prefix}/etc/karing/karing.json`.

Binaries and Docker
-------------------

- Binaries
  - Linux/macOS builds are attached to Releases (tag `v*`) and available as workflow artifacts.
  - Filenames: `karing-ubuntu` (Linux), `karing-macos` (macOS).
- Docker
  - Published to GHCR by CI: `ghcr.io/recelsus/karing` with tags for branch/sha/semver.
  - Container respects env vars: `KARING_CONFIG`, `KARING_DATA`, `KARING_LIMIT`, `KARING_MAX_FILE_BYTES`, `KARING_MAX_TEXT_BYTES`, `KARING_NO_AUTH`, `KARING_TRUSTED_PROXY`, `KARING_ALLOW_LOCALHOST`, `KARING_BASE_PATH`, `KARING_DISABLE_FTS`.

Docs
----

- Build/Install: `docs/build.md`
- Configuration: `docs/config.md`
- Development: `docs/README-dev.md`
- Quick guide: `docs/README-r.md`

Used Libraries
--------------

- Drogon (HTTP framework)
- SQLite3
- JsonCpp
- OpenSSL (crypto)
- CMake / Ninja / GitHub Actions

License
-------

This project is provided under an extremely permissive, “almost-unrestricted” license:

- You may use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the software for any purpose (commercial or non‑commercial), free of charge.
- Attribution is appreciated but not required.
- The software is provided “as is”, without warranty of any kind, express or implied, including but not limited to the warranties of merchantability, fitness for a particular purpose and noninfringement. In no event shall the authors or copyright holders be liable for any claim, damages or other liability.

If you need a SPDX identifier for tooling, treat it as MIT‑like with minimal attribution requirements.

Gaps / Next Ideas
-----------------

- Tests are not included yet (recommend Catch2/CTest).
- Optional rate limiting / request logging middleware.
- Windows CI and container images (if needed).
- Consolidate duplicate health handler (a secondary health method exists in source and could be removed).
