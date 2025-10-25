# memo

ビルド/実行
- Release ビルド
  - `mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j`
- Debug ビルド
  - `mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && make -j`
- インストール（要 sudo）
  - `cd build && sudo make install`
- クリーンビルド
  - `rm -rf build && mkdir build`

起動（ローカル）
- 既定ポート/DB で起動
  - `./build/karing`
- ベースパスを設定して起動
  - `KARING_BASE_PATH=/myapp ./build/karing`
- 制限値の上書き
  - `KARING_LIMIT=100 KARING_MAX_FILE_BYTES=20971520 ./build/karing`
- テスト用（認証無効/localhost 許可）
  - `KARING_NO_AUTH=1 KARING_ALLOW_LOCALHOST=1 ./build/karing`

API キー管理（CLI）
- 一覧表示
  - `./build/karing keys list --data ./karing.db`
- 追加（シークレット自動生成）
  - `./build/karing keys add --label dev --role write --data ./karing.db`
- JSON 形式での追加結果
  - `./build/karing keys add --json --data ./karing.db`
- 有効化/無効化（ID 指定）
  - `./build/karing keys disable 1 --data ./karing.db`
  - `./build/karing keys enable 1 --data ./karing.db`
- ロール/ラベル変更
  - `./build/karing keys set-role 1 read --data ./karing.db`
  - `./build/karing keys set-label 1 backend --data ./karing.db`
- 削除
  - `./build/karing keys rm 1 --hard --data ./karing.db`

IP 制御リスト（CLI）
- 一覧表示
  - `./build/karing ip list --data ./karing.db`
  - `./build/karing ip list deny --data ./karing.db`
- 追加
  - `./build/karing ip add 10.0.0.5/24 allow --data ./karing.db`
- 削除
  - `./build/karing ip del allow:3 --data ./karing.db`

ヘルス/基本操作（curl）
- ヘルス
  - `curl 'http://localhost:8080/health'`
- 最新取得
  - `curl 'http://localhost:8080/?limit=1' -H 'X-API-Key: YOUR_KEY'`
- 検索
  - `curl 'http://localhost:8080/?q=foo*&limit=20' -H 'X-API-Key: YOUR_KEY'`
- テキスト作成
  - `curl -X POST 'http://localhost:8080/' -H 'Content-Type: application/json' -H 'X-API-Key: YOUR_KEY' -d '{"content":"hello"}'`
- ファイル作成
  - `curl -X POST 'http://localhost:8080/' -H 'X-API-Key: YOUR_KEY' -F 'file=@./sample.png' -F 'mime=image/png' -F 'filename=sample.png'`
  

DB/設定
- DB/上限の確認だけして終了
  - `./build/karing --check-db`
- 設定ファイルの探索順（優先）
  - `--config`/`KARING_CONFIG` > `$XDG_CONFIG_HOME/karing/karing.json` > `~/.config/karing/karing.json` > `/etc/karing/karing.json` > `config/karing.json`
- DB 既定パス
  - `$XDG_DATA_HOME/karing/karing.db` / `~/.local/share/karing/karing.db`

Docker（GHCR）
- 実行（8080 公開、永続化）
  - `docker run --rm -p 8080:8080 -v karing-data:/var/lib/karing ghcr.io/recelsus/karing:latest`
- ベースパス設定
  - `docker run --rm -p 8080:8080 -e KARING_BASE_PATH=/myapp ghcr.io/recelsus/karing:latest`
