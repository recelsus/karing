# Build & Install

The following CMake options can be used to choose what to build.

- `karing-server` only
- `karing` CLI only, with either HTTP+SQLite backends or SQLite-only backend
- both

## Dependencies

- C++17 toolchain
- CMake 3.22+
- when building the server:
  - Drogon (dev)
  - SQLite3
  - JsonCpp
- when building the CLI:
  - JsonCpp
  - libcurl, only when `KARING_CLI_ENABLE_HTTP_BACKEND=ON`

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

SQLite-only CLI:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DKARING_BUILD_SERVER=OFF \
  -DKARING_BUILD_CLI=ON \
  -DKARING_CLI_ENABLE_HTTP_BACKEND=OFF
cmake --build build -j
```

- both:
  - `build/server/karing-server`
  - `build/cli/karing`
- server only:
  - `build/server/karing-server`
- CLI only:
  - `build/cli/karing`

## Build Options

- `KARING_BUILD_SERVER`
  - builds `karing-server`
- `KARING_BUILD_CLI`
  - builds `karing`
- `KARING_CLI_ENABLE_HTTP_BACKEND`
  - default: `ON`
  - `ON`: CLI supports HTTP targets and SQLite targets
  - `OFF`: CLI supports SQLite targets only and does not link libcurl

The SQLite-only CLI option is intended for container or embedded distribution jobs that do not need HTTP calls. It is not a restriction on container builds; workflows may still build the normal CLI in a container by leaving `KARING_CLI_ENABLE_HTTP_BACKEND=ON`.

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
- when switching `KARING_CLI_ENABLE_HTTP_BACKEND`, re-run configure
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

The official image sets `KARING_LISTEN=0.0.0.0` explicitly. Local non-Docker server runs default to `127.0.0.1`.
