#!/usr/bin/env bash
set -euo pipefail

BASE_URL=${BASE_URL:-http://localhost:8080}
API_KEY=${KARING_API_KEY:-}
CMD=${1:-list}

headers=()
if [[ -n "$API_KEY" ]]; then
  headers+=(-H "X-API-Key: $API_KEY")
fi

case "$CMD" in
  list)
    curl -fsSL "${headers[@]}" "$BASE_URL/admin/auth"
    ;;
  create-key)
    label=${2:-dev}
    role=${3:-user}
    data="{\"action\":\"create_api_key\",\"label\":\"$label\",\"role\":\"$role\"}"
    curl -fsSL -X POST "${headers[@]}" -H "Content-Type: application/json" -d "$data" "$BASE_URL/admin/auth"
    ;;
  add-ip)
    pattern=${2:-"127.0.0.1/8"}
    permission=${3:-allow}
    data="{\"action\":\"add_ip_rule\",\"pattern\":\"$pattern\",\"permission\":\"$permission\"}"
    curl -fsSL -X POST "${headers[@]}" -H "Content-Type: application/json" -d "$data" "$BASE_URL/admin/auth"
    ;;
  *)
    echo "Usage: $0 {list|create-key <label> <role>|add-ip <cidr> <allow|deny>}" >&2
    exit 1
    ;;
 esac
