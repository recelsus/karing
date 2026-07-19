# Karing

## Description

A pastebin-like API server built on Drogon.
It stores text and files in SQLite and a designated directory, and provides HTTP APIs for create, search, retrieve, update, and delete.
It runs as a single binary and supports reverse proxies under subpaths and Docker.

Put simply, it is for storing notes and pulling them back out.

- C++17 (Drogon)
- SQLite (single file)
- Text and files (see supported MIME types)
- FTS5 search (over text bodies and filenames)

## MIME-TYPE

- text: `text/*`
- image: `image/*`
- audio: `audio/*`
- video: `video/*`
- document: `application/pdf`
- archive: `application/zip`, `application/gzip`, `application/x-tar`, `application/x-7z-compressed`, `application/vnd.rar`
- office:
  - `application/msword`
  - `application/vnd.openxmlformats-officedocument.wordprocessingml.document`
  - `application/vnd.ms-excel`
  - `application/vnd.openxmlformats-officedocument.spreadsheetml.sheet`
  - `application/vnd.ms-powerpoint`
  - `application/vnd.openxmlformats-officedocument.presentationml.presentation`

## Quick Start

- prebuilt binaries
  - Linux and macOS artefacts are uploaded by GitHub Actions. See the latest Release.

- Docker
  - image: `ghcr.io/recelsus/karing:latest`
  - example:
    `docker run -p 8080:8080 ghcr.io/recelsus/karing:latest`
  - the image sets `KARING_LISTEN=0.0.0.0` explicitly

- build
  - see `docs/build.md`

## CLI

`karing` operates against `KARING_TARGET`. Use `--target` to override it for a command.

- `--target http://...` or `--target https://...`
  - HTTP backend
- `--target <path>`
  - local SQLite backend path

Examples:

```bash
karing init-db ./karing.sqlite --limit 100
KARING_TARGET=http://127.0.0.1:8080 karing add "server note"
KARING_TARGET=http://127.0.0.1:8080 karing find server
karing --target ./karing.sqlite add "local note"
```

SQLite targets support text add/get/find/mod/delete, `swap`, `move`, `resequence`, and file metadata display. File upload, file replace, file record deletion, and file body reads are HTTP-backend operations only.

`--url`, `KARING_URL`, and `--id` are not supported. Either `--target` or `KARING_TARGET` must be set.

If a local SQLite database remains locked beyond the busy timeout, plain output reports `ERROR: database is busy`; JSON output returns `E_SQLITE_BUSY`.

## Run Options

- server settings are provided only through CLI options and environment variables
- public deployments should run behind a reverse proxy; TLS termination is expected at the proxy
- default paths:
  - listen: `127.0.0.1:8080`
  - Docker image listen: `0.0.0.0:8080`
  - DB: `/var/lib/karing/karing.sqlite`
  - fallback DB: `$XDG_DATA_HOME/karing/karing.sqlite` if the default location is not available
  - if `XDG_DATA_HOME` is unset: `$HOME/.local/share/karing/karing.sqlite`
  - upload path: `<db directory>/uploads`
  - log: `$XDG_STATE_HOME/karing/logs` (or `$HOME/.local/state/karing/logs`)

- arguments:
  - `KARING_LISTEN`, `KARING_PORT`
  - `KARING_DB_PATH`, `KARING_UPLOAD_PATH`, `KARING_LOG_PATH`
  - `KARING_LIMIT`, `KARING_MAX_FILE`, `KARING_MAX_TEXT`
  - `KARING_BASE_PATH`, `KARING_ERROR_DETAIL`, `KARING_TARGET`

- CLI options:
  - `--listen`, `--port`, `--db-path`, `--upload-path`
  - `--limit`, `--max-file`, `--max-text`
  - `--error-detail`
  - `--max-file` and `--max-text` are specified in MB

- details: see `docs/option.md`

## Endpoints

- `GET /`
  - with no parameters: returns the newest single item as raw output
  - `id=<id>`: returns the specified item as raw output
  - `json=true`: returns a JSON array instead of raw output
  - `as=download`: attachment response, available only with `id`

- `POST /`
  - create a new item
  - `application/json`: `{ "content": "..." }`
  - `multipart/form-data`: file upload
  - response: `201 Created`

- `PUT /?id=<id>`
  - overwrite an existing record
  - because this is treated as modifying an existing record, it is not treated as the latest item

- `PATCH /?id=<id>`
  - partial update
  - JSON: replace text content
  - multipart: rename or replace an existing file/text-file

- `POST /swap?id1=<id>&id2=<id>`
  - swap the contents of two IDs
  - the IDs themselves do not change; only the slot contents are exchanged
  - the response returns the two swapped records as an array

- `POST /move?id=<id>&before=<id>`
  - move one ID before another ID, shifting the records between them
  - for example, moving `5` before `2` changes `a b c d e` into `a e b c d`
  - the response returns the active records and `next_id`

- `POST /resequence`
  - compact active records into `1..n` using `stored_at asc, id asc`
  - move empty slots to the end
  - the response returns the resequenced active records and `next_id`

- `DELETE /`
  - with no `id`: deletes only the latest created record
    - only if it is still within ten minutes of creation

- `DELETE /?id=<id>`
  - delete the specified ID

- `GET /search`
  - list or normal search
  - `q`: search term
  - `limit`: number of returned records
  - `type=text|file`: type filter
  - `sort=id|stored_at|updated_at`
  - `order=asc|desc`
  - default sort/order is `id desc`
  - when `q` is omitted, active records are returned according to `type`, `sort`, `order`, and `limit`

- `GET /search/live`
  - incremental search
  - `q` is required
  - `limit`, `type`, `sort`, and `order` are available
  - default sort/order is `id desc`

- `GET /health`
  - returns service state and DB information as JSON

- when `base_path` is set, the endpoints are also reachable under `<base_path>/`, `<base_path>/swap`, `<base_path>/move`, `<base_path>/resequence`, `<base_path>/search`, `<base_path>/search/live`, and `<base_path>/health`
- when `base_path` is not `/`, requests outside `<base_path>` return `404`
- `base_path` can be a path such as `/karing` or a full URL such as `https://example.test/karing`; only the path part is used

For request and response examples, see `docs/requests.md`.

## Search

- FTS5 is required. The server does not start on a SQLite environment without it.
- `/search` is for normal search, `/search/live` is for incremental search.
- Search targets are text bodies and filenames.

## Documents

- build/install: `docs/build.md`
- CLI: `docs/cli.md`
- options: `docs/option.md`
- request/response: `docs/requests.md`
- development: `docs/README-dev.md`
- quick: `docs/README-r.md`

## Library

- Drogon
- SQLite3
- JsonCpp
- CMake / Ninja / GitHub Actions

## License

MIT License.
