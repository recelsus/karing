# Configuration

Karing ships with compiled defaults, so the binary starts without any JSON files. To customise Drogon listeners, TLS, or logging, pass `--config /path/to/karing.json` and point it to a Drogon-compatible JSON file (a sample lives at `config/karing.json` and is installed to `${prefix}/etc/karing/karing.json`). Only the keys present in that file override the defaults; unknown keys are ignored. Per-field precedence is: compiled defaults < config file (`--config`) < environment variables < CLI flags.

## DB path defaults
- Linux/macOS: `$XDG_DATA_HOME/karing/karing.db` or `~/.local/share/karing/karing.db`
- Windows: `%LOCALAPPDATA%\karing\karing.db`
- When both locations are unavailable, the DB is created next to the executable
- Override with `--data` or `KARING_DATA`

## Key fields (`karing.json`)
- `app`: Drogon app settings; supports `require_tls` and `trusted_proxies`
- `listeners`: address/port and TLS cert/key entries
- `log`: `log_level`, `log_path`
- `client_max_body_size`: HTTP body limit (bytes)
- `karing` (app-specific):
  - `limit`: per-request list limit (clamped by build cap)
  - `max_file_bytes` (≤ 20MiB), `max_text_bytes` (≤ 10MiB)
  - `base_path`: serve endpoints under `<base_path>` in addition to `/`
  - `trusted_proxies`: array or CSV of CIDRs

## Runtime overrides (env/CLI)
- Paths: `KARING_DATA`, `KARING_LOG_PATH`, `KARING_BASE_PATH`, `--data`, `--base-path`
- Limits: `KARING_LIMIT`, `KARING_MAX_FILE_BYTES`, `KARING_MAX_TEXT_BYTES`, `--limit`, `--max-file-bytes`, `--max-text-bytes`
- Auth/proxy flags: `KARING_NO_AUTH`, `KARING_TRUSTED_PROXY`, `KARING_ALLOW_LOCALHOST`, `--no-auth`, `--trusted-proxy`, `--allow-localhost`
- TLS: `--tls`, `--tls-cert`, `--tls-key`, `--require-tls`

## Base path
- Endpoints are exposed at both `/` and `<base_path>` (e.g., `/myapp`, `/myapp/health`, `/myapp/search`)
- Configure it via `karing.json`, `KARING_BASE_PATH`, or `--base-path`
