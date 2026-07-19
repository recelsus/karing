# CLI

`karing` は `KARING_TARGET`、または `--target` で指定した target に対して操作する CLI です。

`http://` または `https://` で始まる `--target` は HTTP backend を使います。
それ以外の target はローカル SQLite path として扱います。

CLI を `KARING_CLI_ENABLE_HTTP_BACKEND=OFF` でビルドした場合、HTTP target は `E_BACKEND` を返し、SQLite target のみ利用できます。

## Target

target は `--target`、次に `KARING_TARGET` の順で解決します。
どちらも未設定の場合、CLI はエラーで終了します。

例:

```bash
karing get 11
karing --target ./karing.sqlite find
```

`--url`、`KARING_URL`、`--id` はサポートしません。

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
- `--target <target>`
- `--json`
- `--help`
- `--version`

## Command

- `karing`
  - 選択されたtargetから最新1件を取得
- `karing get <id>`
  - 指定IDを取得
- `karing add <text>`
  - テキストを追加
- `karing add -f <path> [--mime <type>] [--name <filename>]`
  - ファイルを追加
- `karing mod <id> <text>`
  - 指定IDをテキストで上書き
- `karing mod <id> -f <path> [--mime <type>] [--name <filename>]`
  - 指定IDをファイルで上書き
- `karing del`
  - 作成から10分以内の最新レコードのみ削除
- `karing del <id>`
  - 指定IDの削除
- `karing swap <id1> <id2>`
  - 2つのIDの内容を入れ替え
- `karing move <id> <before-id>`
  - 指定IDを別IDの直前に移動し、間のレコードをずらす
- `karing resequence`
  - active レコードを `1..n` に詰め直す
- `karing find [query] [--limit|-l <n>] [--type|-t text|file] [--sort|-s id|store|update] [--asc] [--desc] [--full]`
  - 一覧または検索
- `karing init-db <path> [--limit|-l <n>] [--force]`
  - SQLite database を明示 path で作成または再初期化/resize

`--target <target>` は `KARING_TARGET` を上書きします。
`karing anything` のような未知の位置引数は text 追加せず help を表示します。

## Runtime Behaviour

- `--json`
  - JSON 応答を表示
- `--target`
  - `KARING_TARGET` を上書きする
- `karing get <id>`
  - text はそのまま表示
  - non-text file は生バイナリを流さず、download 用の `curl` / `wget` 例を表示
- `find`
  - 既定では table 表示
  - `--full` なしでは `content` を省略表示

## SQLite Backend

SQLite target で対応する操作:

- text add/get/find/mod/delete
- `swap`
- `move`
- `resequence`
- file metadata 表示
- `init-db` による明示的な SQLite database 作成
- 直近削除で空いた slot の次回 add での再利用

SQLite target で対応しない操作:

- file upload
- file replace
- file record の削除
- upload directory からの file body 読み取り

SQLite target で file または text-file record を `--json` なしで取得した場合、file body ではなく metadata を表示します。

SQLite database の lock が busy timeout を超えて残る場合、plain output は `ERROR: database is busy`、JSON output は `E_SQLITE_BUSY` を返します。

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
