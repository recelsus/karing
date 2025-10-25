# syntax=docker/dockerfile:1

ARG BUILDER_IMAGE=drogonframework/drogon:latest
ARG RUNTIME_IMAGE=debian:bookworm-slim

FROM ${BUILDER_IMAGE} AS builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential cmake ninja-build git && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j \
 && strip build/karing
RUN set -eux; \
    mkdir -p /opt/runtime/lib; \
    find /usr -name 'libdrogon.so*' -exec cp -a '{}' /opt/runtime/lib/ ';' ; \
    find /usr -name 'libtrantor.so*' -exec cp -a '{}' /opt/runtime/lib/ ';'

FROM ${RUNTIME_IMAGE} AS runtime
ENV DEBIAN_FRONTEND=noninteractive \
    XDG_DATA_HOME=/var/lib/karing \
    KARING_DATA=/var/lib/karing/karing.db \
    KARING_LOG_PATH=/var/log/karing
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libbrotli1 \
    libc-ares2 \
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
    mkdir -p /root/.config/karing /var/lib/karing /var/log/karing
COPY --from=builder /opt/runtime/lib/ /usr/local/lib/
RUN echo /usr/local/lib > /etc/ld.so.conf.d/karing-local.conf && ldconfig
COPY --from=builder /src/build/karing /usr/local/bin/karing
COPY config/karing.json /root/.config/karing/karing.json
EXPOSE 8080
VOLUME ["/var/lib/karing", "/var/log/karing"]
ENTRYPOINT ["/usr/local/bin/karing"]
# Environment variable overrides supported by the app:
# KARING_CONFIG, KARING_DATA, KARING_LIMIT, KARING_MAX_FILE_BYTES,
# KARING_MAX_TEXT_BYTES, KARING_NO_AUTH, KARING_TRUSTED_PROXY,
# KARING_ALLOW_LOCALHOST, KARING_BASE_PATH
