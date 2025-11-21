#!/usr/bin/env bash
set -euo pipefail

BASE_URL=${BASE_URL:-http://localhost:8080}
API_KEY=${KARING_API_KEY:-}
CONTENT=${1:-"Hello from scripts/create_text.sh"}

headers=(-H "Content-Type: application/json")
if [[ -n "$API_KEY" ]]; then
  headers+=(-H "X-API-Key: $API_KEY")
fi

data=$(cat <<JSON
{ "action": "create_text", "content": $(printf %s "$CONTENT" | python3 -c 'import json,sys; print(json.dumps(sys.stdin.read()))') }
JSON
)

curl -fsSL "${headers[@]}" -d "$data" "$BASE_URL/"
