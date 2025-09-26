# Build and Install

This guide covers building Karing from source and installing it. The examples use the classic out‑of‑source flow: `mkdir build && cd build; cmake .. && make`.

Requirements
- C++17 toolchain, CMake 3.22+
- Drogon (dev), SQLite3, JsonCpp, OpenSSL

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
- Build with MSVC and vcpkg (TBD). The app probes `%APPDATA%` and `%LOCALAPPDATA%` for config/data paths.

Dev builds
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j
```

Notes
- Production builds: use `Release` or `RelWithDebInfo`.
- Optional LTO: `-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON`.
- Strip symbols for distribution: `-DCMAKE_INSTALL_DO_STRIP=ON` or `strip build/karing`.

Prebuilt binaries
- GitHub Actions attach Linux/macOS binaries to Releases (tags `v*`) and expose build artifacts for pushes.

Docker
- Official image is published to GHCR: `ghcr.io/recelsus/karing`.
- Example run:
  ```bash
  docker run --rm -p 8080:8080 \
    -e KARING_BASE_PATH=/myapp \
    -v karing-data:/var/lib/karing \
    ghcr.io/recelsus/karing:latest
  ```
