# CLI

`karing` は server endpoint を呼び出すための wrapper CLI です。

## URL Resolution

- `--url`
- `KARING_URL`

優先順は上からです。

例:

```bash
karing --url http://127.0.0.1:8080 health
KARING_URL=http://127.0.0.1:8080 karing find
```

## API Key

- `--api-key`
- `KARING_API_KEY`

優先順は上からです。

CLI は API key が与えられている場合:

- まず `Authorization: Bearer <key>`
- 失敗時に `X-API-Key: <key>`

を試します。成功した方式は一時的にキャッシュされます。

## Global Option

- `--url <url>`
- `--api-key <key>`
- `--json`
- `--id <id>`
- `--help`
- `--version`

## Command

- `karing`
  - 最新1件を取得
- `karing <id>`
  - 指定IDを取得
- `karing --id <id>`
  - 指定IDを取得
- `karing add [text]`
  - テキストを追加
- `karing add -f <path> [--mime <type>] [--name <filename>]`
  - ファイルを追加
- `karing mod <id> [text]`
  - 指定IDをテキストで上書き
- `karing mod <id> -f <path> [--mime <type>] [--name <filename>]`
  - 指定IDをファイルで上書き
- `karing del [id]`
  - 最新の削除または指定IDの削除
- `karing swap <id1> <id2>`
  - 2つのIDの内容を入れ替え
- `karing resequence`
  - active レコードを `1..n` に詰め直す
- `karing find [query] [--limit|-l <n>] [--type|-t text|file] [--sort|-s id|store|update] [--asc] [--desc] [--full]`
  - 一覧または検索
- `karing health`
  - `/health` を表示

## Runtime Behaviour

- `--json`
  - server 応答の生 JSON を表示
- `karing <id>`
  - text はそのまま表示
  - non-text file は生バイナリを流さず、download 用の `curl` / `wget` 例を表示
- `find`
  - 既定では table 表示
  - `--full` なしでは `content` を省略表示

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
