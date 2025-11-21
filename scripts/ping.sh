#!/usr/bin/env bash
set -euo pipefail

BASE_URL=${BASE_URL:-http://localhost:8080}

curl -fsSL "$BASE_URL/" || true
curl -fsSL "$BASE_URL/health" || true
