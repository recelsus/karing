# Karing

## Description

DrogonベースのPastebin風APIサーバー。
テキストやファイルをSQLiteと指定ディレクトリに保存、作成/検索/取得/更新/削除の HTTP API を提供します。
単一バイナリで動作し、リバースプロキシ配下(サブパス)やDockerをサポートします。

平たく言えばメモして取り出すだけです

- C++17(Drogon)
- SQLite(単一ファイル)
- テキストとファイル(対応MIME-TYPE参照)
- FTS5検索(テキスト本文とファイル名を対象)

## MIME-TYPE

- テキスト: `text/*`
- 画像: `image/*`
- 音声: `audio/*`
- 動画: `video/*`
- 文書: `application/pdf`
- アーカイブ: `application/zip`, `application/gzip`, `application/x-tar`, `application/x-7z-compressed`, `application/vnd.rar`
- Office系:
  - `application/msword`
  - `application/vnd.openxmlformats-officedocument.wordprocessingml.document`
  - `application/vnd.ms-excel`
  - `application/vnd.openxmlformats-officedocument.spreadsheetml.sheet`
  - `application/vnd.ms-powerpoint`
  - `application/vnd.openxmlformats-officedocument.presentationml.presentation`

## Quick Start

- 配布バイナリ
  - GitHub Actions によりLinux/macOSの成果物をアップロードしています。最新の Release から取得できます。

- Docker
  - イメージ: `ghcr.io/recelsus/karing:latest`
  - 例:
    `docker run -p 8080:8080 ghcr.io/recelsus/karing:latest`
  - イメージでは `KARING_LISTEN=0.0.0.0` を明示設定しています。

- ビルド
  - `docs/build-ja.md` を参照。

## CLI

`karing` は `KARING_TARGET` に対して操作します。command単位で上書きする場合は `--target` を指定します。

- `--target http://...` または `--target https://...`
  - HTTP backend
- `--target <path>`
  - local SQLite backend path

例:

```bash
karing init-db ./karing.sqlite --limit 100
KARING_TARGET=http://127.0.0.1:8080 karing add "server note"
KARING_TARGET=http://127.0.0.1:8080 karing find server
karing --target ./karing.sqlite add "local note"
```

SQLite target では text add/get/find/mod/delete、`swap`、`move`、`resequence`、file metadata 表示に対応します。file upload、file replace、file record削除、file body読み取りはHTTP backendの操作です。

`--url`、`KARING_URL`、`--id` はサポートしません。`--target` または `KARING_TARGET` のどちらかが必要です。

## Run Options

- サーバー設定は CLI オプションと環境変数だけで与えます。
- 公開利用ではリバースプロキシ配下を推奨し、TLS終端はプロキシ側で行う想定です。
- 既定パス:
  - listen: `127.0.0.1:8080`
  - Docker image listen: `0.0.0.0:8080`
  - DB: `/var/lib/karing/karing.sqlite`
  - フォールバックDB: 既定位置が使えない場合は `$XDG_DATA_HOME/karing/karing.sqlite`
  - `XDG_DATA_HOME` が無ければ `$HOME/.local/share/karing/karing.sqlite`
  - upload path: `<db directory>/uploads`
  - ログ: `$XDG_STATE_HOME/karing/logs`(未設定時は `$HOME/.local/state/karing/logs`)

- aruguments:
  - `KARING_LISTEN`, `KARING_PORT`
  - `KARING_DB_PATH`, `KARING_UPLOAD_PATH`, `KARING_LOG_PATH`
  - `KARING_LIMIT`, `KARING_MAX_FILE`, `KARING_MAX_TEXT`
  - `KARING_BASE_PATH`, `KARING_ERROR_DETAIL`, `KARING_TARGET`

- CLI options:
  - `--listen`, `--port`, `--db-path`, `--upload-path`
  - `--limit`, `--max-file`, `--max-text`
  - `--error-detail`
  - `--max-file` と `--max-text` は MB 指定

- 詳細は `docs/option-ja.md` を参照。

## Endpoints

- `GET /`
  - パラメータ無し: 最も新しい1件をrawで返却
  - `id=<id>`: 指定IDをrawで返却
  - `json=true`: rawではなくJSON配列で返却
  - `as=download`: `id`指定時のみattachmentで返却

- `POST /`
  - 新規作成
  - `application/json`: `{ "content": "..." }`
  - `multipart/form-data`: file upload
  - 返却: `201 Created`

- `PUT /?id=<id>`
  - 既存レコードの上書き
  - 既存レコードの変更という扱いのため最新としては返りません

- `PATCH /?id=<id>`
  - 部分更新
  - JSON: テキスト内容の差し替え
  - multipart: 既存 file/text-file の rename または file 差し替え

- `POST /swap?id1=<id>&id2=<id>`
  - 2つのIDの内容を入れ替え
  - ID自体は変わらず、各スロットの内容だけを交換
  - 応答では入れ替え後の2レコードを配列で返却

- `POST /move?id=<id>&before=<id>`
  - 1つのIDを別IDの直前へ移動し、間のレコードをずらす
  - 例: `5` を `2` の前へ移動すると `a b c d e` は `a e b c d` になる
  - 応答では active レコード配列と `next_id` を返却

- `POST /resequence`
  - active レコードを `stored_at asc, id asc` で `1..n` に詰め直し
  - 空スロットは末尾へ寄せる
  - 応答では振り直し後の active レコード配列と `next_id` を返却

- `DELETE /`
  - `id` なし: 最新作成レコード1件だけを削除 
    - 削除対象は「最新かつ作成から10分以内」の場合のみ
- `DELETE /?id=<id>`
  - 指定IDの削除

- `GET /search`
  - 一覧または通常検索
  - `q`: 検索語
  - `limit`: 返却件数
  - `type=text|file`: 種別絞り込み
  - `sort=id|stored_at|updated_at`
  - `order=asc|desc`
  - 既定 sort/order は `id desc`
  - `q` 省略時は `type` `sort` `order` `limit` に従って active レコード一覧を返却

- `GET /search/live`
  - インクリメンタルサーチ
  - `q` 必須
  - `limit`, `type`, `sort`, `order` を利用可能
  - 既定 sort/order は `id desc`

- `GET /health`
  - サービス状態と DB 情報を JSON で返却

- base_path指定時は `<base_path>/`、`<base_path>/swap`、`<base_path>/move`、`<base_path>/resequence`、`<base_path>/search`、`<base_path>/search/live`、`<base_path>/health` で到達可能。
- `base_path` は `/karing` のような path または `https://example.test/karing` のような URL 全体で指定できます。内部では path 部分だけを使います。

リクエスト例とレスポンス例は `docs/requests-ja.md` を参照してください。

## Search

- FTS5は必須。利用できないSQLite環境では起動しません。
- `/search` は通常検索、`/search/live` はインクリメンタル検索向け。
- 検索対象はテキスト本文とファイル名。

## Documents

- ビルド/インストール: `docs/build-ja.md`
- CLI: `docs/cli-ja.md`
- オプション: `docs/option-ja.md`
- リクエスト/レスポンス: `docs/requests-ja.md`
- 開発: `docs/README-dev.md`
- クイック: `docs/README-r.md`

## Library

- Drogon
- SQLite3
- JsonCpp
- CMake / Ninja / GitHub Actions

## License

MIT License.
