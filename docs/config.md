# Configuration

Karing ships with compiled defaults, so the binary starts without any JSON files. To customise Drogon listeners, TLS, or logging, pass `--config /path/to/karing.json` and point it to a Drogon-compatible JSON file (a sample lives at `config/karing.json` and is installed to `${prefix}/etc/karing/karing.json`). Only the keys present in that file override the defaults; unknown keys are ignored. Per-field precedence is: compiled defaults < environment variables < CLI flags (including `--config`). Field-specific CLI flags such as `--port` or `--limit` always win over values from `--config`.

## DB path defaults
- Linux/macOS: `$XDG_DATA_HOME/karing/karing.db` or `~/.local/share/karing/karing.db`
- Windows: `%LOCALAPPDATA%\karing\karing.db`
- When both locations are unavailable, the DB is created next to the executable
- Override with `--data` or `KARING_DATA`

## Key fields (`karing.json`)
- `app`: Drogon app settings; supports `require_tls` and `trusted_proxies`
- `listeners`: address/port, TLS cert/key entries, and optional `base_url` (single object or array; lone objects are auto-wrapped)
- `log`: `log_level`, `log_path`
- `storage` (app-specific):
  - `limit`: per-request list limit (clamped by build cap)
  - `upload_limit`: HTTP body limit (bytes); propagated to Drogon's `client_max_body_size`
  - `trusted_proxies`: array or CSV of CIDRs
  - `web_enabled`: toggle the `/web` placeholder endpoint
- Legacy fields such as top-level `client_max_body_size` or `karing.*` are still accepted but rewritten into `storage.*` at load time

## Runtime overrides (env/CLI)
- Paths: `KARING_DATA`, `KARING_LOG_PATH`, `KARING_BASE_URL` (alias: `KARING_BASE_PATH`), `--data`, `--base-path`, `--baseurl`, `--base-url`
- Limits: `KARING_LIMIT`, `--limit`
- Auth/proxy flags: `KARING_NO_AUTH`, `KARING_TRUSTED_PROXY`, `KARING_ALLOW_LOCALHOST`, `--no-auth`, `--trusted-proxy`, `--allow-localhost`
- TLS: `--tls`, `--tls-cert`, `--tls-key`, `--require-tls`
- Web UI toggle: `KARING_WEB_UI`, `--enable-web`, `--disable-web`, `storage.web_enabled`
- Log level: `KARING_LOG_LEVEL`, `--log-level`, `log.log_level` (`NONE` disables app logs)

## Base URL prefix
- Endpoints are exposed at both `/` and `<base_url>` (e.g., `/myapp`, `/myapp/health`, `/myapp/search`)
- Configure it via `listeners[].base_url`, `KARING_BASE_URL` (`KARING_BASE_PATH` alias), or `--base-path`/`--base-url`
