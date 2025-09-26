# 設定

Karing は Drogon 互換の JSON 設定ファイル `karing.json` を利用し、CLI/環境変数で上書きできます。

探索順
- `--config <path>` / `KARING_CONFIG`
- `$XDG_CONFIG_HOME/karing/karing.json`
- `~/.config/karing/karing.json`
- `/etc/karing/karing.json`（POSIX）
- `config/karing.json`（同梱デフォルト）
- Windows: `%APPDATA%\karing\karing.json` も探索

DB の既定パス
- Linux/macOS: `$XDG_DATA_HOME/karing/karing.db` または `~/.local/share/karing/karing.db`
- Windows: `%LOCALAPPDATA%\karing\karing.db`
- `--data` / `KARING_DATA` で上書き可能

主な項目（karing.json）
- `app`: Drogon のアプリ設定。`require_tls`, `trusted_proxies` をサポート
- `listeners`: `address`/`port` と TLS `cert`/`key`（リバースプロキシ未使用時）
- `log`: `log_level`, `log_path`
- `client_max_body_size`: HTTP ボディ上限（バイト）
- `karing`（アプリ固有）:
  - `limit`: 取得上限（ビルド時上限でクランプ）
  - `max_file_bytes`（≤ 20MiB）, `max_text_bytes`（≤ 10MiB）
  - `base_path`: サブパス配下で提供（`<base_path>`, `<base_path>/health`, `<base_path>/restore`）
  - `trusted_proxies`: CIDR 配列または CSV

実行時の上書き（環境変数/CLI）
- サイズ/上限: `KARING_LIMIT`, `KARING_MAX_FILE_BYTES`, `KARING_MAX_TEXT_BYTES`
- 認証/プロキシ: `KARING_NO_AUTH`, `KARING_ALLOW_LOCALHOST`, `KARING_TRUSTED_PROXY`
- パス: `KARING_BASE_PATH`, `KARING_CONFIG`, `KARING_DATA`
- TLS: `--tls`, `--tls-cert`, `--tls-key`, `--require-tls`

ベースパス
- ルート `/` と `<base_path>` の両方でエンドポイントを提供（例: `/myapp`, `/myapp/health`, `/myapp/restore`）。
- 通常は `karing.json` に設定し、必要に応じて CLI/ENV で上書きしてください。
