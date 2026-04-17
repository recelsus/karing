# CLI

`karing` is a wrapper CLI for calling the server endpoints.

## URL Resolution

- `--url`
- `KARING_URL`

The precedence is top to bottom.

Example:

```bash
karing --url http://127.0.0.1:8080 health
KARING_URL=http://127.0.0.1:8080 karing find
```

## API Key

- `--api-key`
- `KARING_API_KEY`

The precedence is top to bottom.

When an API key is provided, the CLI tries:

- `Authorization: Bearer <key>`
- `X-API-Key: <key>` when the first form fails

The successful scheme is cached temporarily.

## Global Option

- `--url <url>`
- `--api-key <key>`
- `--json`
- `--id <id>`
- `--help`
- `--version`

## Command

- `karing`
  - fetch the latest record
- `karing <id>`
  - fetch the specified ID
- `karing --id <id>`
  - fetch the specified ID
- `karing add [text]`
  - add text
- `karing add -f <path> [--mime <type>] [--name <filename>]`
  - add a file
- `karing mod <id> [text]`
  - overwrite the specified ID with text
- `karing mod <id> -f <path> [--mime <type>] [--name <filename>]`
  - overwrite the specified ID with a file
- `karing del [id]`
  - delete the latest record or the specified ID
- `karing swap <id1> <id2>`
  - swap the contents of two IDs
- `karing resequence`
  - compact active records into `1..n`
- `karing find [query] [--limit|-l <n>] [--type|-t text|file] [--sort|-s id|store|update] [--asc] [--desc] [--full]`
  - list or search
- `karing health`
  - show `/health`

## Runtime Behaviour

- `--json`
  - print the raw JSON response from the server
- `karing <id>`
  - text is printed directly
  - non-text files do not dump raw binary; the CLI prints `curl` / `wget` download examples instead
- `find`
  - uses a table view by default
  - without `--full`, the `content` column is truncated

## Example

```bash
KARING_URL=http://127.0.0.1:8080 karing
KARING_URL=http://127.0.0.1:8080 karing --id 5
KARING_URL=http://127.0.0.1:8080 karing add "hello"
echo "hello" | KARING_URL=http://127.0.0.1:8080 karing add
KARING_URL=http://127.0.0.1:8080 karing add -f ./note.txt
KARING_URL=http://127.0.0.1:8080 karing mod 5 "updated"
KARING_URL=http://127.0.0.1:8080 karing del
KARING_URL=http://127.0.0.1:8080 karing swap 3 4
KARING_URL=http://127.0.0.1:8080 karing resequence
KARING_URL=http://127.0.0.1:8080 karing find test --limit 10 --sort id --desc
KARING_URL=http://127.0.0.1:8080 karing health
```
