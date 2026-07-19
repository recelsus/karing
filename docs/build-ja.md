# Build & Install

CMake option で以下を切り替えられます。

- `karing-server` のみ
- `karing` CLI のみ。HTTP+SQLite backend または SQLite-only backend
- 両方

## Dependencies

- C++17 ツールチェイン
- CMake 3.22+
- server をビルドする場合:
  - Drogon（dev）
  - SQLite3
  - JsonCpp
- CLI をビルドする場合:
  - JsonCpp
  - libcurl は `KARING_CLI_ENABLE_HTTP_BACKEND=ON` の場合のみ

#### Linux (Ubuntu)

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake \
  libdrogon-dev libsqlite3-dev libjsoncpp-dev libcurl4-openssl-dev zlib1g-dev
```

#### macOS (Homebrew)

```bash
brew update
brew install drogon curl jsoncpp sqlite3
```

## Build

両方:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DKARING_BUILD_SERVER=ON \
  -DKARING_BUILD_CLI=ON
cmake --build build -j
```

serverのみ:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DKARING_BUILD_SERVER=ON \
  -DKARING_BUILD_CLI=OFF
cmake --build build -j
```

CLIのみ:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DKARING_BUILD_SERVER=OFF \
  -DKARING_BUILD_CLI=ON
cmake --build build -j
```

SQLite-only CLI:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DKARING_BUILD_SERVER=OFF \
  -DKARING_BUILD_CLI=ON \
  -DKARING_CLI_ENABLE_HTTP_BACKEND=OFF
cmake --build build -j
```

- 両方:
  - `build/server/karing-server`
  - `build/cli/karing`
- serverのみ:
  - `build/server/karing-server`
- CLIのみ:
  - `build/cli/karing`

## Build Options

- `KARING_BUILD_SERVER`
  - `karing-server` をビルドする
- `KARING_BUILD_CLI`
  - `karing` をビルドする
- `KARING_CLI_ENABLE_HTTP_BACKEND`
  - 既定値: `ON`
  - `ON`: CLI は HTTP target と SQLite target に対応する
  - `OFF`: CLI は SQLite target のみに対応し、libcurl をリンクしない

SQLite-only CLI option は、HTTP 呼び出しが不要な container / embedded 配布 job 向けです。container build の制約ではありません。workflow 側で `KARING_CLI_ENABLE_HTTP_BACKEND=ON` のままにすれば、container 内でも通常版 CLI をビルドできます。

## Test

```bash
cmake -S . -B build -DBUILD_TESTING=ON \
  -DKARING_BUILD_SERVER=ON \
  -DKARING_BUILD_CLI=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

SQLite-only CLI build smoke test:

```bash
cmake -S . -B build-cli-sqlite-only -DCMAKE_BUILD_TYPE=Release \
  -DKARING_BUILD_SERVER=OFF \
  -DKARING_BUILD_CLI=ON \
  -DKARING_CLI_ENABLE_HTTP_BACKEND=OFF \
  -DBUILD_TESTING=OFF
cmake --build build-cli-sqlite-only -j
```

## Notes

- `server only`、`CLI only`、`both` を切り替える場合は同じ `build/` directory に対して再 configure する
- `KARING_CLI_ENABLE_HTTP_BACKEND` を切り替える場合も再 configure する
- production では `Release` または `RelWithDebInfo` を推奨する
