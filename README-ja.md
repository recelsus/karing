Karing
=============

Karing は Drogon ベースの軽量な Pastebin 風 API サーバー。
テキストやファイル(画像・音声)を SQLite に保存し、作成/検索/取得/更新/削除の HTTP API を提供します。単一バイナリで動作し、API キー認証、IP 制御、リバースプロキシ配下（サブパス）や Docker をサポートします。

- C++17（Drogon）
- SQLite（単一ファイル）
- テキストとファイル（画像/音声）
- FTS5 検索（テキスト本文とファイル名を対象）
- API キー認証 + IP allow/deny
- ベースパスでサブパス公開
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
      -e KARING_BASE_PATH=/myapp \
      -e KARING_LIMIT=100 \
      -v karing-data:/var/lib/karing \
      ghcr.io/recelsus/karing:latest
    ```
- ソースからビルド
  - `docs/build-ja.md` を参照（Release/Debug、インストール、各 OS 注意点）。

設定
----

- 設定ファイル: Drogon 互換 JSON（`karing.json`）
- 探索順（後ろはフォールバック）:
  - `--config <path>` / `KARING_CONFIG`
  - `$XDG_CONFIG_HOME/karing/karing.json`
  - `~/.config/karing/karing.json`
  - `/etc/karing/karing.json`（POSIX）
  - `config/karing.json`（同梱デフォルト）
- Windows
  - 設定: `%APPDATA%\karing\karing.json`
  - 既定 DB: `%LOCALAPPDATA%\karing\karing.db`
- 環境変数が設定を上書き可能（`KARING_BASE_PATH`, `KARING_LIMIT` など）。詳細は `docs/config-ja.md`。

エンドポイント
--------------

- `GET /` — 最新1件をRAWで返却（テキスト: text/plain、ファイル: inline）。`id=` 指定でそのIDをRAW。`json=true` でJSON返却。
- `POST /` — 作成（JSON `{ content }` または multipart）。201 で `{ success: true, message: "Created", id }` を返却。
- `PUT /?id=` — 全置換
- `PATCH /?id=` — 部分更新
- `DELETE /?id=` — 論理削除
- `GET /health` — サービス情報
- `GET|POST /search` — 一覧/検索（JSON）
  - パラメータ無し: 最新から `limit` 件（既定は runtime_limit）
  - `limit`
  - `q` — FTS5 クエリ（テキスト: content、ファイル: filename）
  - `type` — `text` | `file`（省略時は混在）
  - 返却: `{ success: true, message: "OK", data: [...], meta: { count, limit, total? } }`
  - 認可: `GET /search` は read で許可。`POST /search` も read として扱います（write 権限は不要）。

備考
- ベースパス指定時は `<base_path>`、`<base_path>/health`、`<base_path>/restore`、`<base_path>/search` でも到達可能。
- 認証は `X-API-Key` / `?api_key=`（ロール: read/write）。`docs/config-ja.md` を参照。

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
- 既定設定: `${prefix}/etc/karing/karing.json`

配布物（バイナリ / Docker）
---------------------------

- バイナリ: Release（タグ v*）に添付 / Actions の Artifacts から取得可能
  - ファイル名: `karing-ubuntu`（Linux）, `karing-macos`（macOS）
- Docker: GHCR `ghcr.io/recelsus/karing`（branch/sha/semver タグ）
  - コンテナは以下の環境変数を認識: `KARING_CONFIG`, `KARING_DATA`, `KARING_LIMIT`, `KARING_MAX_FILE_BYTES`, `KARING_MAX_TEXT_BYTES`, `KARING_NO_AUTH`, `KARING_TRUSTED_PROXY`, `KARING_ALLOW_LOCALHOST`, `KARING_BASE_PATH`, `KARING_DISABLE_FTS`

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
