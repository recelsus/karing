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

- Built-in defaults keep the binary runnable without any files: listens on `0.0.0.0:8080`, limit 100, logs in `./logs`, and the SQLite file is created next to the binary (or the XDG locations below when available).
- Config file (optional): pass `--config /path/to/karing.json` to load a Drogon-compatible JSON. A sample lives at `config/karing.json`; `make install` also drops it at `${prefix}/etc/karing/karing.json`. Only the provided keys override defaults (unknown keys are ignored), so you can keep the file minimal.
- Runtime precedence per field: compiled defaults < config file (`--config`) < environment variables < CLI flags. CLI overrides such as `--port`/`--tls` always win.
- Defaults (XDG):
  - DB: `$XDG_DATA_HOME/karing/karing.db` or `~/.local/share/karing/karing.db` if `XDG_DATA_HOME` is unset. When both fail, `karing.db` is created next to the executable.
  - Logs: `$XDG_STATE_HOME/karing/logs` or `~/.local/state/karing/logs` if `XDG_STATE_HOME` is unset.
- Windows paths
  - DB default: `%LOCALAPPDATA%\karing\karing.db`
- Env shortcuts:
  - Paths: `KARING_DATA`, `KARING_LOG_PATH`, `KARING_BASE_PATH`
  - Limits: `KARING_LIMIT`, `KARING_MAX_FILE_BYTES`, `KARING_MAX_TEXT_BYTES`
  - Flags: `KARING_NO_AUTH`, `KARING_TRUSTED_PROXY`, `KARING_ALLOW_LOCALHOST`
- Details: see `docs/config.md`.

Endpoints
----------------------

- `GET /` — raw latest (text/plain or inline file). With `id=`, returns that item inline. Add `json=true` to return JSON instead.
- `POST /` — multi-purpose endpoint. Provide an `action` parameter (query, JSON field, or form field).
  - JSON (`application/json`): default `action=create_text`. Supply `{ content }` to create text. Use `action=update_text`/`patch_text` with `id` to replace or patch existing text. `action=delete` with `id` performs logical delete.
  - Multipart (`multipart/form-data`): default `action=create_file`. Upload a file field to create file entries. Use `action=update_file`/`patch_file` with `id` to replace or patch files. `action=delete` with `id` is also accepted via multipart form fields.
  - Responses mirror previous verbs: creates return `{ success: true, message: "Created", id }` (201), other actions return `{ success: true, ... }` or 204 for delete.
- `GET /health` — service/build/limits/TLS/base_path
- `GET|POST /search` — list/search API (JSON only)
  - No params: latest items up to `limit` (default: runtime limit)
  - `limit`
  - `q` — FTS5 query (text: content, file: filename)
  - `type` — `text` | `file` (optional filter)
  - Response: `{ success: true, message: "OK", data: [...], meta: { count, limit, total? } }`
  - Auth: `GET /search` is open to user-level keys; `POST /search` is also treated as read-only (no admin role required).
- `GET /web` — placeholder endpoint for the future Web UI bundle (currently returns a JSON stub).

Notes
- Base path support: the same endpoints are available under `<base_path>`, e.g., `/myapp`, `/myapp/health`, `/myapp/search`.
- Auth: API key via `X-API-Key` or `?api_key=`, role‑based (user/admin). See `docs/config.md`.

Auth Policy
-----------

- Role hierarchy: `user < admin`.
- IP precedence (`ip_rules` permission column):
  - `deny` match → always reject (even with a valid API key).
  - `allow` match → bypass auth (accepted regardless of API key).
  - neither → require API key with sufficient role.
- Required roles by endpoint:
  - `GET /`, `GET /health`, `GET/POST /search`, `POST /` (any action) → `user` or `admin`
  - `GET /admin/auth` → `admin` (unless IP allow bypass applies)

Admin CLI (API Keys & IP Control)
---------------------------------

API key management and IP allow/deny lists can be managed via the `karing` CLI.

- API keys
  - `karing keys add --label "CI from repo A"`                   # Auto-generate and add an API key. Default role=user; attach label
  - `karing keys add --role admin --label "ops emergency"`       # Generate an admin key (role=admin)
  - `karing keys add --disabled --label "staged key"`            # Generate but start disabled (pre-rollout staging)
  - `karing keys set-role 42 user`                                # Downgrade existing key (id=42) to user role
  - `karing keys set-role 42 admin`                               # Promote existing key (id=42) to admin
  - `karing keys set-label 42 "CI from repo B"`                   # Update label for existing key (id=42)
  - `karing keys disable 42`                                      # Disable existing key (enabled=0), reversible
  - `karing keys enable 42`                                       # Re-enable a previously disabled key (enabled=1)
  - `karing keys rm 42`                                           # Remove key (logical by default; use --hard for physical delete)
  - `karing keys rm 42 --hard`                                    # Physically delete key (fully removed from DB)
  - `karing keys add --label "will show secret once" --json`      # Output result as JSON; secret is shown only on creation
    # → {"id":..., "role":"user","label":"...","enabled":1,"secret":"..."}

- IP rules (single table with `permission=allow|deny`)
  - `karing ip add 203.0.113.0/24 allow`                          # Add CIDR to allow rules (stored normalized)
  - `karing ip add 203.0.113.10 deny`                             # Add a single IPv4 to deny rules
  - `karing ip add 192.168.1.5/24 allow`                          # Host/prefix format is normalized to network address (192.168.1.0/24)
  - `karing ip del 12`                                            # Remove rule id=12 (prefix `allow:` or `deny:` also accepted)
  - `karing ip add 203.0.113.10/32 deny`                          # Overlaps with allow entries; deny wins at evaluation
    # At evaluation time, the most specific prefix still wins; this /32 deny takes precedence

Admin Endpoints
---------------

- `GET /admin/auth` (admin)
  - Returns current auth state: API keys and IP lists.
  - Response shape:
    ```json
    {
      "api_keys": [
        {"id":1,"key":"...","label":"...","enabled":true,"role":"user","created_at":...,"last_used_at":...,"last_ip":"..."}
      ],
      "ip_rules": [
        {"id":12, "pattern":"203.0.113.0/24", "permission":"allow", "enabled":true, "created_at":...},
        {"id":13, "pattern":"203.0.113.10/32", "permission":"deny", "enabled":true, "created_at":...}
      ]
    }
    ```
  - Use the `id` values with `karing ip del <id>` (prefixes like `allow:<id>` remain accepted for backward compatibility).

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
- Installs binary to `${prefix}/bin/karing` and a sample config to `${prefix}/etc/karing/karing.json` (load it with `--config`).

Binaries and Docker
-------------------

- Binaries
  - Linux/macOS builds are attached to Releases (tag `v*`) and available as workflow artifacts.
  - Filenames: `karing-ubuntu` (Linux), `karing-macos` (macOS).
- Docker
  - Published to GHCR by CI: `ghcr.io/recelsus/karing` with tags for branch/sha/semver.
  - Container respects env vars: `KARING_DATA`, `KARING_LOG_PATH`, `KARING_LIMIT`, `KARING_MAX_FILE_BYTES`, `KARING_MAX_TEXT_BYTES`, `KARING_NO_AUTH`, `KARING_TRUSTED_PROXY`, `KARING_ALLOW_LOCALHOST`, `KARING_BASE_PATH`, `KARING_DISABLE_FTS`.
  - To use a JSON config, bind-mount it into the container (e.g., `/etc/karing/karing.json`) and append `--config /etc/karing/karing.json` to the run command.
  - Defaults baked in Dockerfile: `KARING_DATA=/var/lib/karing/karing.db`, `KARING_LOG_PATH=/var/log/karing` (override with `-e` or compose `environment:`).

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
