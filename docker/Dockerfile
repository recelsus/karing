# syntax=docker/dockerfile:1

ARG BUILDER_IMAGE=drogonframework/drogon:latest
ARG RUNTIME_IMAGE=debian:bookworm-slim

FROM ${BUILDER_IMAGE} AS builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential cmake ninja-build git \
    libcurl4-openssl-dev \
    libjsoncpp-dev \
    pkg-config && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DKARING_BUILD_SERVER=ON \
    -DKARING_BUILD_CLI=ON \
 && cmake --build build -j \
 && strip build/server/karing-server \
 && strip build/cli/karing
RUN set -eux; \
    mkdir -p /opt/runtime/lib; \
    find /usr -name 'libdrogon.so*' -exec cp -a '{}' /opt/runtime/lib/ ';' ; \
    find /usr -name 'libtrantor.so*' -exec cp -a '{}' /opt/runtime/lib/ ';'

FROM ${RUNTIME_IMAGE} AS runtime
ENV DEBIAN_FRONTEND=noninteractive \
    XDG_DATA_HOME=/var/lib/karing \
    KARING_DB_PATH=/var/lib/karing/karing.sqlite \
    KARING_UPLOAD_PATH=/var/lib/karing/uploads \
    KARING_LOG_PATH=/var/log/karing \
    KARING_URL=http://127.0.0.1:8080
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libbrotli1 \
    libc-ares2 \
    libcurl4 \
    libjsoncpp25 \
    libpq5 \
    libmariadb3 \
    libhiredis0.14 \
    libsqlite3-0 \
    libssl3 \
    libstdc++6 \
    libuuid1 \
    sqlite3 && \
    rm -rf /var/lib/apt/lists/* && \
    mkdir -p /var/lib/karing /var/log/karing /app
COPY --from=builder /opt/runtime/lib/ /usr/local/lib/
RUN echo /usr/local/lib > /etc/ld.so.conf.d/karing-local.conf && ldconfig
COPY --from=builder /src/build/server/karing-server /usr/local/bin/karing-server
COPY --from=builder /src/build/cli/karing /usr/local/bin/karing
WORKDIR /app
EXPOSE 8080
VOLUME ["/var/lib/karing", "/var/log/karing"]
ENTRYPOINT ["/usr/local/bin/karing-server"]
# Environment variable overrides supported by the app:
# KARING_DB_PATH, KARING_UPLOAD_PATH, KARING_LOG_PATH,
# KARING_LISTEN, KARING_PORT, KARING_LIMIT, KARING_MAX_FILE, KARING_MAX_TEXT, KARING_BASE_PATH
