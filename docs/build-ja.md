# ビルドとインストール

この文書は Karing をソースからビルド/インストールする手順です。例は従来の out-of-source 手順（`mkdir build && cd build; cmake .. && make`）で統一しています。

要件
- C++17 ツールチェイン, CMake 3.22+
- Drogon（dev）, SQLite3, JsonCpp, OpenSSL

Linux (Ubuntu)
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake \
  libdrogon-dev libsqlite3-dev libjsoncpp-dev libssl-dev zlib1g-dev
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
sudo make install
```

macOS (Homebrew)
```bash
brew update && brew install drogon
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
sudo make install
```

Windows
- MSVC + vcpkg を想定（TBD）。設定/データの既定パスは `%APPDATA%` / `%LOCALAPPDATA%` を探索します。

開発ビルド
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j
```

注意
- 本番は `Release` または `RelWithDebInfo` を推奨
- 任意: `-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON`（LTO）
- 署名/シンボル除去: `-DCMAKE_INSTALL_DO_STRIP=ON` または `strip build/karing`

配布バイナリ
- GitHub Actions が Linux/macOS の成果物を Release（タグ `v*`）に添付し、push の artifacts も提供します。

Docker
- GHCR の公式イメージ: `ghcr.io/recelsus/karing`
- 例:
  ```bash
  docker run --rm -p 8080:8080 \
    -e KARING_BASE_PATH=/myapp \
    -v karing-data:/var/lib/karing \
    ghcr.io/recelsus/karing:latest
  ```

