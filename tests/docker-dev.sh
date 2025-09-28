#!/usr/bin/env bash
set -euo pipefail

# Simple local build-and-run helper for Docker
# - Builds local image
# - Removes existing container and volume
# - Recreates volume and runs container with a known-good config

IMAGE_TAG=${IMAGE_TAG:-karing:dev}
CONTAINER_NAME=${CONTAINER_NAME:-karing}
VOLUME_NAME=${VOLUME_NAME:-karing-dev-data}
HOST_PORT=${HOST_PORT:-8080}

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CONFIG_PATH="$SCRIPT_DIR/local.karing.json"

echo "[i] Building image: $IMAGE_TAG"
docker build -t "$IMAGE_TAG" -f "$REPO_ROOT/docker/Dockerfile" "$REPO_ROOT"

echo "[i] Stopping/removing container if exists: $CONTAINER_NAME"
docker rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || true

echo "[i] Removing volume if exists: $VOLUME_NAME"
docker volume rm -f "$VOLUME_NAME" >/dev/null 2>&1 || true

echo "[i] Creating volume: $VOLUME_NAME"
docker volume create "$VOLUME_NAME" >/dev/null

# Ensure local config exists
if [[ ! -f "$CONFIG_PATH" ]]; then
  echo "[i] Writing local config: $CONFIG_PATH"
  cat >"$CONFIG_PATH" <<'JSON'
{
  "app": { "name": "karing", "threads": 1 },
  "listeners": [ { "address": "0.0.0.0", "port": 8080, "https": false } ],
  "log": { "log_level": "INFO", "log_path": "./logs" },
  "client_max_body_size": 20971520,
  "karing": { "limit": 10, "max_file_bytes": 20971520, "max_text_bytes": 10485760 },
  "db_clients": []
}
JSON
fi

echo "[i] Running container on :$HOST_PORT (volume=$VOLUME_NAME)"
exec docker run --rm \
  --name "$CONTAINER_NAME" \
  -p "$HOST_PORT:8080" \
  -v "$VOLUME_NAME:/var/lib/karing" \
  -v "$CONFIG_PATH:/etc/karing/karing.json:ro" \
  -e KARING_DATA=/var/lib/karing/karing.db \
  ${KARING_LIMIT:+-e KARING_LIMIT=$KARING_LIMIT} \
  ${KARING_DISABLE_FTS:+-e KARING_DISABLE_FTS=$KARING_DISABLE_FTS} \
  -e KARING_NO_AUTH=1 \
  -e KARING_ALLOW_LOCALHOST=1 \
  "$IMAGE_TAG"
