# CLI

`karing` is a CLI for operating against a target.

Targets beginning with `http://` or `https://` use the HTTP backend.
Any other target is treated as a local SQLite path.

When the CLI is built with `KARING_CLI_ENABLE_HTTP_BACKEND=OFF`, HTTP targets return `E_BACKEND` and only SQLite targets are available.

## Target

The target is the first positional argument.

Examples:

```bash
karing http://127.0.0.1:8080 health
karing ./karing.sqlite find
```

`--url` and `KARING_URL` are not supported.

## API Key

- `--api-key`
- `KARING_API_KEY`

The precedence is top to bottom.

When an API key is provided, the CLI tries:

- `Authorization: Bearer <key>`
- `X-API-Key: <key>` when the first form fails

The successful scheme is cached temporarily.

## Global Option

- `--api-key <key>`
- `--json`
- `--error-detail`
- `--id <id>`
- `--help`
- `--version`

## Command

- `karing <target>`
  - fetch the latest record
- `karing <target> <id>`
  - fetch the specified ID
- `karing --id <id> <target>`
  - fetch the specified ID
- `karing <target> add [text]`
  - add text
- `karing <target> add -f <path> [--mime <type>] [--name <filename>]`
  - add a file
- `karing <target> mod <id> [text]`
  - overwrite the specified ID with text
- `karing <target> mod <id> -f <path> [--mime <type>] [--name <filename>]`
  - overwrite the specified ID with a file
- `karing <target> del [id]`
  - delete the latest record or the specified ID
- `karing <target> swap <id1> <id2>`
  - swap the contents of two IDs
- `karing <target> resequence`
  - compact active records into `1..n`
- `karing <target> find [query] [--limit|-l <n>] [--type|-t text|file] [--sort|-s id|store|update] [--asc] [--desc] [--full]`
  - list or search
- `karing <target> health`
  - show `/health`
- `karing init-db <path> [--limit|-l <n>] [--force]`
  - create or explicitly reinitialize/resize a SQLite database

## Runtime Behaviour

- `--json`
  - print the raw JSON response from the server
- `--error-detail` or `KARING_ERROR_DETAIL=1`
  - include internal error details in local CLI errors
- `karing <id>`
  - text is printed directly
  - non-text files do not dump raw binary; the CLI prints `curl` / `wget` download examples instead
- `find`
  - uses a table view by default
  - without `--full`, the `content` column is truncated

## SQLite Backend

SQLite targets support:

- text add/get/find/mod/delete
- `swap`
- `resequence`
- `health`
- file metadata display
- explicit SQLite database creation through `init-db`

SQLite targets do not support:

- file upload
- file replace
- deleting file records
- reading file bodies from the upload directory

When a file or text-file record is fetched from a SQLite target without `--json`, the CLI prints metadata instead of the file body.

## Example

```bash
karing init-db ./karing.sqlite --limit 100
karing http://127.0.0.1:8080
karing http://127.0.0.1:8080 5
karing http://127.0.0.1:8080 add "hello"
echo "hello" | karing http://127.0.0.1:8080 add
karing http://127.0.0.1:8080 add -f ./note.txt
karing http://127.0.0.1:8080 mod 5 "updated"
karing http://127.0.0.1:8080 del
karing http://127.0.0.1:8080 swap 3 4
karing http://127.0.0.1:8080 resequence
karing http://127.0.0.1:8080 find test --limit 10 --sort id --desc
karing http://127.0.0.1:8080 health
karing ./karing.sqlite health
karing ./karing.sqlite add "local note"
karing ./karing.sqlite find local
```
