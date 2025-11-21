#!/usr/bin/env bash
set -euo pipefail

BASE_URL=${BASE_URL:-http://localhost:8080}
API_KEY=${KARING_API_KEY:-}
QUERY=${1:-}
LIMIT=${2:-10}

url="$BASE_URL/search?limit=$LIMIT"
if [[ -n "$QUERY" ]]; then
  url+="&q=$(python3 -c 'import urllib.parse,sys; print(urllib.parse.quote(sys.argv[1]))' "$QUERY")"
fi

headers=()
if [[ -n "$API_KEY" ]]; then
  headers+=(-H "X-API-Key: $API_KEY")
fi

curl -fsSL "${headers[@]}" "$url"
