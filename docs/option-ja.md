# Option

サーバー設定は CLI オプションと環境変数のみで与えます。

## 既定値

- listen: `0.0.0.0:8080`

- DB path:
  - `--db-path` 指定時はその値
  - 未指定時は `/var/lib/karing/karing.sqlite`
  - `/var/lib/karing/karing,sqlite` が使えない場合は `$XDG_DATA_HOME/karing/karing.sqlite`
  - `XDG_DATA_HOME` が無ければ `~/.local/share/karing/karing.sqlite`

- upload path:
  - `--upload-path` 指定時はその値
  - DB path が `/var/lib` からフォールバックした場合は `$XDG_DATA_HOME/karing/uploads`
  - `XDG_DATA_HOME` が無ければ `~/.local/share/karing/uploads`
  - それ以外は `<db directory>/uploads`

- log path:
  - `KARING_LOG_PATH` があればそれを使用
  - なければ `$XDG_STATE_HOME/karing/logs` または `~/.local/state/karing/logs`

## CLI Options

- `--listen <host>`
- `--port <n>`
- `--db-path <path>`
- `--max-text <mb>`
- `--max-file <mb>`
- `--limit <n>`
- `--upload-path <path>`
- `--check-db`
- `--init-db`

## Environment

- listen: `KARING_LISTEN`, `KARING_PORT`
- path: `KARING_DB_PATH`, `KARING_UPLOAD_PATH`, `KARING_LOG_PATH`
- 上限: `KARING_LIMIT`, `KARING_MAX_FILE`, `KARING_MAX_TEXT`
  - `KARING_MAX_FILE` と `KARING_MAX_TEXT`はMBとして扱う(例: KARING_MAX_TEXT=1 (= 1MB))
- base path: `KARING_BASE_PATH`
- `KARING_BASE_PATH` を設定すると、エンドポイントは `<base_path>` 配下で利用できます。

## Command Sample

```bash
./karing --init-db --db-path /var/lib/karing/karing.sqlite --limit 100
./karing --listen 127.0.0.1 --port 8080 --db-path ./karing.sqlite --upload-path ./uploads
KARING_BASE_PATH=/karing KARING_PORT=8080 ./karing
```
