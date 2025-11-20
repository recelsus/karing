# 設定

Karing は JSON を用意しなくても動作できるようデフォルトを全てバイナリに埋め込んでいます。Drogon 互換の `karing.json` を読み込みたい場合は `--config /path/to/karing.json` を明示的に指定してください（サンプルは `config/karing.json` に同梱、`make install` では `${prefix}/etc/karing/karing.json` に配置されます）。既定の階層に存在しないキーは無視され、記述した項目のみ上書きされます。優先度は「組込みデフォルト < 設定ファイル (`--config`) < 環境変数 < 実行時オプション」です。

## DB path defaults
- Linux/macOS: `$XDG_DATA_HOME/karing/karing.db` または `~/.local/share/karing/karing.db`
- Windows: `%LOCALAPPDATA%\karing\karing.db`
- 上記が得られない場合は実行ファイルと同じディレクトリに `karing.db`
- `--data` / `KARING_DATA` で上書き可能

## 主な項目 (`karing.json`)
- `app`: Drogon のアプリ設定（`require_tls`, `trusted_proxies` など）
- `listeners`: 受信アドレス/ポート、TLS `cert`/`key`
- `log`: `log_level`, `log_path`
- `client_max_body_size`: HTTP リクエストボディ上限
- `karing`（アプリ固有）:
  - `limit`: `/search` 等の取得件数（ビルド上限でクランプ）
  - `max_file_bytes` (≤20MiB), `max_text_bytes` (≤10MiB)
  - `base_path`: `/myapp` のようなサブパス提供
  - `trusted_proxies`: CIDR 配列または CSV

## Runtime overrides
- パス: `KARING_DATA`, `KARING_LOG_PATH`, `KARING_BASE_PATH`, `--data`, `--base-path`
- 上限: `KARING_LIMIT`, `KARING_MAX_FILE_BYTES`, `KARING_MAX_TEXT_BYTES`, `--limit`, `--max-file-bytes`, `--max-text-bytes`
- フラグ: `KARING_NO_AUTH`, `KARING_TRUSTED_PROXY`, `KARING_ALLOW_LOCALHOST`, `--no-auth`, `--trusted-proxy`, `--allow-localhost`
- TLS: `--tls`, `--tls-cert`, `--tls-key`, `--require-tls`

## Base path
- `/` と `<base_path>` の両方でエンドポイントを提供（例: `/myapp`, `/myapp/health`, `/myapp/search`）
- `karing.json`、`KARING_BASE_PATH`、`--base-path` のいずれでも設定できます
