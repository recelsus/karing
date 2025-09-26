# syntax=docker/dockerfile:1

FROM drogonframework/drogon:latest AS builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential cmake ninja-build git && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j

FROM drogonframework/drogon:latest AS runtime
ENV DEBIAN_FRONTEND=noninteractive \
    XDG_CONFIG_HOME=/etc \
    XDG_DATA_HOME=/var/lib/karing
RUN apt-get update && apt-get install -y \
    sqlite3 && \
    rm -rf /var/lib/apt/lists/* && \
    mkdir -p /etc/karing /var/lib/karing /var/log/karing
COPY --from=builder /src/build/karing /usr/local/bin/karing
COPY config/karing.json /etc/karing/karing.json
EXPOSE 8080
VOLUME ["/var/lib/karing", "/var/log/karing"]
ENTRYPOINT ["/usr/local/bin/karing"]
# Environment variable overrides supported by the app:
# KARING_CONFIG, KARING_DATA, KARING_LIMIT, KARING_MAX_FILE_BYTES,
# KARING_MAX_TEXT_BYTES, KARING_NO_AUTH, KARING_TRUSTED_PROXY,
# KARING_ALLOW_LOCALHOST, KARING_BASE_PATH
