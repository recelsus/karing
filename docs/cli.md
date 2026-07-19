# CLI

`karing` is a CLI for operating against `KARING_TARGET`, or against a target passed with `--target`.

`--target` values beginning with `http://` or `https://` use the HTTP backend.
Any other target value is treated as a local SQLite path.

When the CLI is built with `KARING_CLI_ENABLE_HTTP_BACKEND=OFF`, HTTP targets return `E_BACKEND` and only SQLite targets are available.

## Target

The target is resolved from `--target`, then `KARING_TARGET`.
If neither is set, the CLI exits with an error.

Examples:

```bash
karing get 11
karing --target ./karing.sqlite find
```

`--url`, `KARING_URL`, and `--id` are not supported.

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
- `--target <target>`
- `--json`
- `--help`
- `--version`

## Command

- `karing`
  - fetch the latest record from the selected target
- `karing get <id>`
  - fetch the specified ID
- `karing add <text>`
  - add text
- `karing add -f <path> [--mime <type>] [--name <filename>]`
  - add a file
- `karing mod <id> <text>`
  - overwrite the specified ID with text
- `karing mod <id> -f <path> [--mime <type>] [--name <filename>]`
  - overwrite the specified ID with a file
- `karing del`
  - delete only the latest record when it is still within ten minutes of creation
- `karing del <id>`
  - delete the specified ID
- `karing swap <id1> <id2>`
  - swap the contents of two IDs
- `karing move <id> <before-id>`
  - move the specified ID before another ID, shifting the records between them
- `karing resequence`
  - compact active records into `1..n`
- `karing find [query] [--limit|-l <n>] [--type|-t text|file] [--sort|-s id|store|update] [--asc] [--desc] [--full]`
  - list or search
- `karing init-db <path> [--limit|-l <n>] [--force]`
  - create or explicitly reinitialize/resize a SQLite database

Use `--target <target>` with the same commands to override `KARING_TARGET`.
Unknown positional strings such as `karing anything` print help instead of adding text.

## Runtime Behaviour

- `--json`
  - print the raw JSON response from the server
- `--target`
  - overrides `KARING_TARGET`
- `karing get <id>`
  - text is printed directly
  - non-text files do not dump raw binary; the CLI prints `curl` / `wget` download examples instead
- `find`
  - uses a table view by default
  - without `--full`, the `content` column is truncated

## SQLite Backend

SQLite targets support:

- text add/get/find/mod/delete
- `swap`
- `move`
- `resequence`
- file metadata display
- explicit SQLite database creation through `init-db`
- reuse of a recently deleted empty slot on the next add

SQLite targets do not support:

- file upload
- file replace
- deleting file records
- reading file bodies from the upload directory

When a file or text-file record is fetched from a SQLite target without `--json`, the CLI prints metadata instead of the file body.

## Example

```bash
karing init-db ./karing.sqlite --limit 100
karing --target ./karing.sqlite add "local note"
KARING_TARGET=./karing.sqlite karing
KARING_TARGET=./karing.sqlite karing get 5
KARING_TARGET=http://127.0.0.1:8080 karing
KARING_TARGET=http://127.0.0.1:8080 karing get 5
KARING_TARGET=http://127.0.0.1:8080 karing add "hello"
echo "hello" | KARING_TARGET=http://127.0.0.1:8080 karing add
KARING_TARGET=http://127.0.0.1:8080 karing add -f ./note.txt
KARING_TARGET=http://127.0.0.1:8080 karing mod 5 "updated"
KARING_TARGET=http://127.0.0.1:8080 karing del
KARING_TARGET=http://127.0.0.1:8080 karing del 5
KARING_TARGET=http://127.0.0.1:8080 karing swap 3 4
KARING_TARGET=http://127.0.0.1:8080 karing move 5 2
KARING_TARGET=http://127.0.0.1:8080 karing resequence
KARING_TARGET=http://127.0.0.1:8080 karing find test --limit 10 --sort id --desc
karing --target ./karing.sqlite add "local note"
karing --target ./karing.sqlite find local
```
