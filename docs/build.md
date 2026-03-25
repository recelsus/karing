# Build & Install

The following CMake options can be used to choose what to build.

- `karing-server` only
- `karing` CLI only
- both

## Dependencies

- C++17 toolchain
- CMake 3.22+
- when building the server:
  - Drogon (dev)
  - SQLite3
  - JsonCpp
- when building the CLI:
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

both:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DKARING_BUILD_SERVER=ON \
  -DKARING_BUILD_CLI=ON
cmake --build build -j
```

server only:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DKARING_BUILD_SERVER=ON \
  -DKARING_BUILD_CLI=OFF
cmake --build build -j
```

CLI only:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DKARING_BUILD_SERVER=OFF \
  -DKARING_BUILD_CLI=ON
cmake --build build -j
```

- both:
  - `build/server/karing-server`
  - `build/cli/karing`
- server only:
  - `build/server/karing-server`
- CLI only:
  - `build/cli/karing`

## Test

```bash
cmake -S . -B build -DBUILD_TESTING=ON \
  -DKARING_BUILD_SERVER=ON \
  -DKARING_BUILD_CLI=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Install

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DKARING_BUILD_SERVER=ON \
  -DKARING_BUILD_CLI=ON
cmake --build build -j
sudo cmake --install build --prefix /usr/local
```

## Notes

- when switching between `server only`, `CLI only`, and `both`, re-run configure against the same `build/` directory
- `Release` or `RelWithDebInfo` is recommended for production use
- `-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON` may be used if desired
- `-DCMAKE_INSTALL_DO_STRIP=ON` may be considered for distribution builds

#### Windows

- MSVC + vcpkg is assumed (TBD)

## Prebuilt Binaries

- GitHub Actions artefacts / Release attachments can be used
- `karing-server` and `karing` can now be handled as separate artefacts

## Docker

- Official GHCR image: `ghcr.io/recelsus/karing`
- example:

```bash
docker run --rm -p 8080:8080 \
  -e KARING_BASE_PATH=/myapp \
  -v karing-data:/var/lib/karing \
  ghcr.io/recelsus/karing:latest
```
