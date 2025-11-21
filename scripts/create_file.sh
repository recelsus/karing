#!/usr/bin/env bash
set -euo pipefail

BASE_URL=${BASE_URL:-http://localhost:8080}
API_KEY=${KARING_API_KEY:-}
FILE_PATH=${1:-"$(dirname "$0")/../docs/logo.png"}
MIME=${2:-image/png}
NAME=${3:-"upload.png"}
ACTION=${4:-create_file}

if [[ ! -f "$FILE_PATH" ]]; then
  echo "file not found: $FILE_PATH" >&2
  exit 1
fi

args=(-F "action=$ACTION" -F "mime=$MIME" -F "filename=$NAME" -F "file=@$FILE_PATH")
if [[ -n "$API_KEY" ]]; then
  args+=(-H "X-API-Key: $API_KEY")
fi

curl -fsSL -X POST "$BASE_URL/" "${args[@]}"
