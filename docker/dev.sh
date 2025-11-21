#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
compose_file="${script_dir}/docker-compose.dev.yml"
service_name="karing-dev"

if [[ ! -f "${compose_file}" ]]; then
  echo "compose file not found: ${compose_file}" >&2
  exit 1
fi

compose_cmd=(docker compose -f "${compose_file}")

usage() {
  cat <<'USAGE'
Usage: docker/dev.sh [command]

Commands:
  up        Build image and start container (default)
  build     Only build the local image
  down      Stop and remove dev container/volumes
  logs      Follow logs from the dev container
  sh        Exec into the running container with /bin/sh
USAGE
}

action=${1:-up}

case "${action}" in
  up)
    "${compose_cmd[@]}" build
    "${compose_cmd[@]}" up -d
    ;;
  build)
    "${compose_cmd[@]}" build
    ;;
  down)
    "${compose_cmd[@]}" down --remove-orphans
    ;;
  logs)
    "${compose_cmd[@]}" logs -f
    ;;
  sh)
    "${compose_cmd[@]}" exec -it "${service_name}" /bin/sh
    ;;
  -h|--help|help)
    usage
    ;;
  *)
    echo "Unknown command: ${action}" >&2
    usage >&2
    exit 1
    ;;
esac
