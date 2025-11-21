Scripts
=======

`scripts/` contains simple shell helpers for manually testing a running karing instance on `http://localhost:8080`.

The scripts assume that:
- The server is running locally and accessible at `http://localhost:8080`.
- API keys (for endpoints that require them) are provided via the `KARING_API_KEY` environment variable.
- JSON payloads use `jq` when needed; the scripts fall back to plain heredocs to avoid extra deps.

Scripts
-------

- `ping.sh` — `GET /` (raw) and `GET /health` to verify the service is up.
- `create_text.sh` — Create a simple text entry via `POST /`.
- `create_file.sh` — Upload a small file (PNG by default) via `POST /` multipart.
- `search.sh` — Query `GET /search` with optional `q` and `limit` arguments.
- `admin.sh` — Issue `GET /admin/auth` or simple `POST /admin/auth` actions (requires IP allow-list and access).

Usage examples
--------------

```bash
# Ping service
./scripts/ping.sh

# Create a text entry
./scripts/create_text.sh "hello from script"

# Upload a file
./scripts/create_file.sh ./docs/logo.png image/png "sample.png"

# Search latest entries
./scripts/search.sh "hello" 5

# Admin list (requires IP allow-listed access)
./scripts/admin.sh list
```
