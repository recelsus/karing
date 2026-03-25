# Build & Install

CMake option で以下を切り替えられます。

- `karing-server` のみ
- `karing` CLI のみ
- 両方

## Dependencies

- C++17 ツールチェイン
- CMake 3.22+
- server をビルドする場合:
  - Drogon（dev）
  - SQLite3
  - JsonCpp
- CLI をビルドする場合:
  - libcurl
  - JsonCpp

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

- 両方:
  - `build/server/karing-server`
  - `build/cli/karing`
- serverのみ:
  - `build/server/karing-server`
- CLIのみ:
  - `build/cli/karing`
