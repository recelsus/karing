# CLI

`karing` は target に対して操作する CLI です。

`http://` または `https://` で始まる target は HTTP backend を使います。
それ以外の target はローカル SQLite path として扱います。

CLI を `KARING_CLI_ENABLE_HTTP_BACKEND=OFF` でビルドした場合、HTTP target は `E_BACKEND` を返し、SQLite target のみ利用できます。

## Target

target は最初の位置引数です。

例:

```bash
karing http://127.0.0.1:8080 health
karing ./karing.sqlite find
```

`--url` と `KARING_URL` はサポートしません。

## API Key

- `--api-key`
- `KARING_API_KEY`

優先順は上からです。

CLI は API key が与えられている場合:

- まず `Authorization: Bearer <key>`
- 失敗時に `X-API-Key: <key>`

を試します。成功した方式は一時的にキャッシュされます。

## Global Option

- `--api-key <key>`
- `--json`
- `--error-detail`
- `--id <id>`
- `--help`
- `--version`

## Command

- `karing <target>`
  - 最新1件を取得
- `karing <target> <id>`
  - 指定IDを取得
- `karing --id <id> <target>`
  - 指定IDを取得
- `karing <target> add [text]`
  - テキストを追加
- `karing <target> add -f <path> [--mime <type>] [--name <filename>]`
  - ファイルを追加
- `karing <target> mod <id> [text]`
  - 指定IDをテキストで上書き
- `karing <target> mod <id> -f <path> [--mime <type>] [--name <filename>]`
  - 指定IDをファイルで上書き
- `karing <target> del [id]`
  - 最新の削除または指定IDの削除
- `karing <target> swap <id1> <id2>`
  - 2つのIDの内容を入れ替え
- `karing <target> resequence`
  - active レコードを `1..n` に詰め直す
- `karing <target> find [query] [--limit|-l <n>] [--type|-t text|file] [--sort|-s id|store|update] [--asc] [--desc] [--full]`
  - 一覧または検索
- `karing <target> health`
  - `/health` を表示
- `karing init-db <path> [--limit|-l <n>] [--force]`
  - SQLite database を明示 path で作成または再初期化/resize

## Runtime Behaviour

- `--json`
  - JSON 応答を表示
- `--error-detail` または `KARING_ERROR_DETAIL=1`
  - local CLI error に内部詳細を含める
- `karing <id>`
  - text はそのまま表示
  - non-text file は生バイナリを流さず、download 用の `curl` / `wget` 例を表示
- `find`
  - 既定では table 表示
  - `--full` なしでは `content` を省略表示

## SQLite Backend

SQLite target で対応する操作:

- text add/get/find/mod/delete
- `swap`
- `resequence`
- `health`
- file metadata 表示
- `init-db` による明示的な SQLite database 作成

SQLite target で対応しない操作:

- file upload
- file replace
- file record の削除
- upload directory からの file body 読み取り

SQLite target で file または text-file record を `--json` なしで取得した場合、file body ではなく metadata を表示します。

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
