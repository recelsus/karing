Karing
=============

Karing は Drogon ベースの軽量な Pastebin 風 API サーバー。
テキストやファイル(画像・音声)を SQLite に保存し、作成/検索/取得/更新/削除の HTTP API を提供します。単一バイナリで動作し、API キー認証、IP 制御、リバースプロキシ配下（サブパス）や Docker をサポートします。

- C++17（Drogon）
- SQLite（単一ファイル）
- テキストとファイル（画像/音声）
- FTS5 検索（テキスト本文とファイル名を対象）
- API キー認証 + IP allow/deny
- ベースURLでサブパス公開
- TLS はリバースプロキシ or アプリで

英語版 README は `README.md` を参照してください。

クイックスタート
-----------------

- 配布バイナリ
  - GitHub Actions により Linux/macOS の成果物をアップロードしています。最新の Release から取得できます。
- Docker（簡易試用に推奨）
  - イメージ: `ghcr.io/recelsus/karing:latest`
  - 例:
    ```bash
    docker run --rm -p 8080:8080 \
      -e KARING_BASE_URL=/myapp \
      -e KARING_LIMIT=100 \
      -v karing-data:/var/lib/karing \
      ghcr.io/recelsus/karing:latest
    ```
- ソースからビルド
  - `docs/build-ja.md` を参照（Release/Debug、インストール、各 OS 注意点）。

設定
----

- バイナリは `0.0.0.0:8080`, limit=100, `./logs` などのデフォルトを内蔵しており、設定ファイルなしで起動できます。
- 設定ファイル（任意）: `--config /path/to/karing.json` を指定した場合のみ Drogon 互換 JSON を読み込みます。テンプレートは `config/karing.json` に同梱され、`make install` では `${prefix}/etc/karing/karing.json` に配置されます（使用時は常に `--config` を渡してください）。既定の階層に存在しないキーは無視され、記述した項目のみ上書きされます。
- 優先度: 組込みデフォルト < 環境変数 < 実行時オプション（`--config` を含む）。設定ファイルを読み込んでも、対応する CLI 引数（例: `--port`, `--limit`）があればそちらが優先されます。
- 既定パス（XDG 準拠）:
  - DB: `$XDG_DATA_HOME/karing/karing.db` または `~/.local/share/karing/karing.db`
  - ログ: `$XDG_STATE_HOME/karing/logs` または `~/.local/state/karing/logs`
  - どちらも得られない場合は実行ファイルと同じディレクトリ
- HTTP のアップロード上限は `storage.upload_limit` のみで制御します（Drogon の `client_max_body_size` に伝播し、DB 側の別制限はありません）。
- Windows
  - 既定 DB: `%LOCALAPPDATA%\karing\karing.db`
- 環境変数の例:
  - パス系: `KARING_CONFIG`, `KARING_DATA`, `KARING_LOG_PATH`, `KARING_BASE_URL`（`KARING_BASE_PATH` は互換エイリアス）
  - 制限系: `KARING_LIMIT`
  - フラグ: `KARING_NO_AUTH`, `KARING_TRUSTED_PROXY`, `KARING_ALLOW_LOCALHOST`, `KARING_WEB_UI`
  - ログ: `KARING_LOG_LEVEL`（`TRACE`/`DEBUG`/`INFO`/`WARN`/`ERROR`/`FATAL`/`NONE`）
- CLI オプション: `--enable-web` / `--disable-web`, `--log-level <level>`
- JSON 設定: `storage.web_enabled` で Web UI を有効/無効化、`storage.upload_limit` でボディ上限、`log.log_level` でログレベル（`NONE` で無効化）を指定できます。
- 詳細は `docs/config-ja.md` を参照


エンドポイント
--------------

- `GET /` — 最新1件をRAWで返却（テキスト: text/plain、ファイル: inline）。`id=` 指定でそのIDをRAW。`json=true` でJSON返却。
- `POST /` — 単一のマルチアクションエンドポイント。`action` パラメータ（クエリ/JSON/フォーム）で動作を指定します。
  - JSON (`application/json`): 既定は `action=create_text`。`{ content }` でテキスト作成。`action=update_text`/`patch_text` + `id` で更新/部分更新、`action=delete` + `id` で削除。
  - Multipart (`multipart/form-data`): 既定は `action=create_file`。ファイルフィールドを送ると作成、`action=update_file`/`patch_file` + `id` で差し替え/部分更新、`action=delete` + `id` で削除。
  - レスポンスは従来同様で、作成は `201 Created`, 変更は `200 OK`, 削除は `204 No Content`。
- `GET /health` — サービス情報
- `GET|POST /search` — 一覧/検索（JSON）
  - パラメータ無し: 最新から `limit` 件（既定は runtime_limit）
  - `limit`
  - `q` — FTS5 クエリ（テキスト: content、ファイル: filename）
  - `type` — `text` | `file`（省略時は混在）
  - 返却: `{ success: true, message: "OK", data: [...], meta: { count, limit, total? } }`
  - 認可: `GET /search` は user ロールで許可。`POST /search` も読み取り扱い（admin 権限は不要）。
- `GET /web` — 将来提供予定の Web UI コンテンツ用プレースホルダー（現在はダミー JSON を返却）。

備考
- ベースURL指定時は `<base_url>`、`<base_url>/health`、`<base_url>/search` でも到達可能。
- 認証は `X-API-Key` / `?api_key=`（ロール: user/admin）。`docs/config-ja.md` を参照。

認可ポリシー
------------

- 役割の序列: `user < admin`（右が上位）。
- IPの優先（`ip_rules` テーブルの `permission` 列）:
  - `deny` に一致 → 常に拒否（APIキーが正しくても拒否）。
  - `allow` に一致 → 認証を開始せず許可（APIキーの有無/正否は不問）。
  - どちらでもない → APIキーを要求（十分なロールが必要）。
- 管理エンドポイント（`/admin/auth`）は `ip_rules` の `permission=allow` に登録された IP のみ許可され、APIキーは無視されます。
- エンドポイント毎の要件:
  - `GET /`, `GET /health`, `GET/POST /search`, `POST /`（任意の action）→ `user` 以上
  - `/admin/auth` → allow に登録した IP のみ

管理CLI（APIキー / IP制御）
--------------------------

`karing` バイナリから APIキー と IP許可/拒否リストを操作できます。

- APIキー
  - `karing key add --label "CI from repo A"`                   # APIキーを自動生成して追加。デフォルトrole=user、ラベルを付与
  - `karing key add --role admin --label "ops emergency"`       # 管理者権限(admin)のキーを自動生成して追加
  - `karing key add --disabled --label "staged key"`            # 生成だけして無効化状態で作成（ロールアウト前の準備）
  - `karing key set-role 42 user`                                # 既存キー(id=42)を user ロールに降格
  - `karing key set-role 42 admin`                               # 既存キー(id=42)を admin ロールに昇格
  - `karing key set-label 42 "CI from repo B"`                   # 既存キー(id=42)のラベルを更新
  - `karing key disable 42`                                      # 既存キー(id=42)を無効化（enabled=0）※復活可能
  - `karing key enable 42`                                       # 無効化したキーを再度有効化（enabled=1）
  - `karing key rm 42`                                           # 既存キー(id=42)を削除（既定は論理削除でなく物理なら--hardを付ける）
  - `karing key rm 42 --hard`                                    # 既存キー(id=42)を物理削除（DBから完全に除去）
  - `karing key add --label "will show secret once" --json`      # 生成結果をJSONで受け取る（secretは初回のみ出力）
    # → {"id":..., "role":"user","label":"...","enabled":1,"secret":"..."}

- IPルール（単一テーブルで `permission=allow|deny`）
  - `karing ip add 203.0.113.0/24 allow`                          # 203.0.113.0/24 を許可ルールに追加（CIDRは正規化）
  - `karing ip add 203.0.113.10 deny`                             # 単一IPv4を拒否ルールに追加
  - `karing ip add 192.168.1.5/24 allow`                          # ホスト/プレフィクス形式 → ネットワークアドレス(192.168.1.0/24)に丸めて保存
  - `karing ip del 12`                                            # ルールID 12 を削除（`allow:12` のような旧形式も可）
  - `karing ip add 203.0.113.10/32 deny`                          # 既存の allow と重なっても追加可（deny が優先）
    # （評価時は“最も具体的なプレフィクス”が優先され、この /32 の deny が勝つ）

管理エンドポイント
------------------

- `GET /admin/auth`（admin）
  - 現在の認証設定（APIキー一覧、IP許可/拒否リスト）を返却。
  - レスポンス構造:
    ```json
    {
      "api_keys": [
        {"id":1,"key":"...","label":"...","enabled":true,"role":"user","created_at":...,"last_used_at":...,"last_ip":"..."}
      ],
      "ip_rules": [
        {"id":12, "pattern":"203.0.113.0/24", "permission":"allow", "enabled":true, "created_at":...},
        {"id":13, "pattern":"203.0.113.10/32", "permission":"deny", "enabled":true, "created_at":...}
      ]
    }
    ```
  - 表示された `id` は `karing ip del <id>` で削除できます（互換のため `allow:<id>` 形式も利用可）。
- `POST /admin/auth` — Web UI から利用する設定管理 API。`action` と各種フィールドを JSON で渡します。
  - APIキー: `create_api_key`, `set_api_key_role`, `set_api_key_label`, `set_api_key_enabled`（または `enable_api_key` / `disable_api_key`）, `delete_api_key`
  - IPルール: `add_ip_rule`, `update_ip_rule`, `delete_ip_rule`

レスポンス形式
--------------

- 成功: `{ success: true, message: "OK" | "Created", ... }`
- 失敗: `{ success: false, code, message, details? }`

検索とFTS
--------

- SQLite の FTS5 仮想テーブル `karing_fts` を使用（カラム: テキストは `content`、ファイルは `filename`）。
- `/search` は GET クエリまたは POST JSON で同じフィールド（q/limit/offset/type）を受け取ります。
- `KARING_DISABLE_FTS=1` を設定すると FTS を無効化（`q` 付き `/search` は 503）。`q` 無しの最新一覧は動作します。
- 前方一致の強化: `KARING_FTS_PREFIX` に `"2 3"` のような値を設定すると、2文字/3文字のプレフィックス索引を有効化できます（例: `hel*` のような先頭一致が高速化）。

インストール（make install）
----------------------------

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --install build --prefix /usr/local
```
- バイナリ: `${prefix}/bin/karing`
- サンプル設定: `${prefix}/etc/karing/karing.json`（利用時は `--config` で指定）

配布物（バイナリ / Docker）
---------------------------

- バイナリ: Release（タグ v*）に添付 / Actions の Artifacts から取得可能
  - ファイル名: `karing-ubuntu`（Linux）, `karing-macos`（macOS）
- Docker: GHCR `ghcr.io/recelsus/karing`（branch/sha/semver タグ）
  - コンテナは以下の環境変数を認識: `KARING_CONFIG`, `KARING_DATA`, `KARING_LOG_PATH`, `KARING_LIMIT`, `KARING_NO_AUTH`, `KARING_TRUSTED_PROXY`, `KARING_ALLOW_LOCALHOST`, `KARING_BASE_PATH`, `KARING_DISABLE_FTS`, `KARING_WEB_UI`, `KARING_LOG_LEVEL`
  - `--config` を使う場合は設定ファイルをバインドマウントし、`--config /etc/karing/karing.json` のように明示的に渡してください。
  - Dockerfile 既定: `KARING_DATA=/var/lib/karing/karing.db`, `KARING_LOG_PATH=/var/log/karing`（`-e` または compose の `environment:` で上書き可）

ドキュメント
------------

- ビルド/インストール: `docs/build-ja.md`
- 設定: `docs/config-ja.md`
- 開発: `docs/README-dev.md`
- クイック: `docs/README-r.md`

利用ライブラリ
--------------

- Drogon（HTTP フレームワーク）
- SQLite3
- JsonCpp
- OpenSSL（crypto）
- CMake / Ninja / GitHub Actions

ライセンス
----------

- 目的（商用/非商用）を問わず、無償で使用・複製・改変・結合・頒布・サブライセンス・販売が可能です。
- クレジット表記は任意。
- 本ソフトウェアは「現状のまま」提供され、商品性・特定目的適合性・非侵害を含むいかなる保証も行いません。作者は一切の責任を負いません。

ツールの都合で SPDX が必要な場合は、MIT 互換（表記任意）として扱って差し支えありません。

不足点 / 今後の候補
--------------------

- テスト未同梱（Catch2/CTest を推奨）
- レート制限 / リクエストロギングのフィルタ
- Windows CI / コンテナイメージ（必要に応じて）
- 健康チェック重複実装の整理（ソース内の重複箇所を削除可能）
