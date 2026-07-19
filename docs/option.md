# Option

Server settings are provided only through CLI options and environment variables.

## Defaults

- listen: `127.0.0.1:8080`
  - Docker image explicitly sets `KARING_LISTEN=0.0.0.0`

- DB path:
  - use the value passed via `--db-path`, if specified
  - otherwise use `/var/lib/karing/karing.sqlite`
  - if `/var/lib/karing/karing.sqlite` is not available, fall back to `$XDG_DATA_HOME/karing/karing.sqlite`
  - if `XDG_DATA_HOME` is not set, fall back to `~/.local/share/karing/karing.sqlite`

- upload path:
  - use the value passed via `--upload-path`, if specified
  - if the DB path fell back from `/var/lib`, use `$XDG_DATA_HOME/karing/uploads`
  - if `XDG_DATA_HOME` is not set, use `~/.local/share/karing/uploads`
  - otherwise use `<db directory>/uploads`

- log path:
  - use `KARING_LOG_PATH` if it is set
  - otherwise use `$XDG_STATE_HOME/karing/logs` or `~/.local/state/karing/logs`

## CLI Options

- `--listen <host>`
- `--port <n>`
- `--db-path <path>`
- `--max-text <mb>`
- `--max-file <mb>`
- `--limit <n>`
- `--upload-path <path>`
- `--error-detail`
- `--check-db`
- `--init-db`

## Environment

- listen: `KARING_LISTEN`, `KARING_PORT`
- path: `KARING_DB_PATH`, `KARING_UPLOAD_PATH`, `KARING_LOG_PATH`
- limits: `KARING_LIMIT`, `KARING_MAX_FILE`, `KARING_MAX_TEXT`
  - `KARING_MAX_FILE` and `KARING_MAX_TEXT` are treated as MB values
  - example: `KARING_MAX_TEXT=1` means `1MB`
- base path: `KARING_BASE_PATH`
- if `KARING_BASE_PATH` is set, endpoints are available under `<base_path>`
- `KARING_BASE_PATH` accepts either a path such as `/karing` or a full URL such as `https://example.test/karing`; only the path part is used
- error detail: `KARING_ERROR_DETAIL=1`
  - includes internal details in structured error responses

## Network

The server defaults to loopback and is intended to be placed behind a reverse proxy for public access.
Terminate TLS at the reverse proxy.

## Command Sample

```bash
./karing --init-db --db-path /var/lib/karing/karing.sqlite --limit 100
./karing --listen 127.0.0.1 --port 8080 --db-path ./karing.sqlite --upload-path ./uploads
KARING_LISTEN=0.0.0.0 ./karing --port 8080
KARING_BASE_PATH=/karing KARING_PORT=8080 ./karing
KARING_BASE_PATH=https://example.test/karing KARING_PORT=8080 ./karing
KARING_ERROR_DETAIL=1 ./karing --port 8080
```
