# Configuration

Karing uses a Drogon‑compatible JSON file named `karing.json`, plus CLI/env overrides.

Discovery order
- `--config <path>` or `KARING_CONFIG`
- `$XDG_CONFIG_HOME/karing/karing.json`
- `~/.config/karing/karing.json`
- `/etc/karing/karing.json` (POSIX)
- `config/karing.json` (bundled default)
- Windows: also `%APPDATA%\karing\karing.json`

DB path defaults
- Linux/macOS: `$XDG_DATA_HOME/karing/karing.db` or `~/.local/share/karing/karing.db`
- Windows: `%LOCALAPPDATA%\karing\karing.db`
- Override with `--data` or `KARING_DATA`.

Key fields (karing.json)
- `app`: Drogon app settings; optional `require_tls` and `trusted_proxies` supported
- `listeners`: address/port and TLS cert/key (when not using reverse proxy)
- `log`: `log_level`, `log_path`
- `client_max_body_size`: HTTP body limit (bytes)
- `karing` (app‑specific):
  - `limit`: per-request list limit (clamped by build cap)
  - `max_file_bytes` (≤ 20MiB), `max_text_bytes` (≤ 10MiB)
  - `base_path`: mount under a subpath; endpoints also available under `<base_path>`
  - `trusted_proxies`: array or CSV of CIDRs

Runtime overrides (env/CLI)
- Size/limits: `KARING_LIMIT`, `KARING_MAX_FILE_BYTES`, `KARING_MAX_TEXT_BYTES`
- Auth/proxy: `KARING_NO_AUTH`, `KARING_ALLOW_LOCALHOST`, `KARING_TRUSTED_PROXY`
- Paths: `KARING_BASE_PATH`, `KARING_CONFIG`, `KARING_DATA`
- TLS: `--tls`, `--tls-cert`, `--tls-key`, `--require-tls`

Base path
- The same endpoints are served at `/` and `<base_path>` (e.g., `/myapp`, `/myapp/health`, `/myapp/search`).
- Prefer configuring it in `karing.json`; CLI/ENV take precedence when present.
