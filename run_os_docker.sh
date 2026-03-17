#!/usr/bin/env bash
#
# Run ThunderOS inside the supported Docker environment.
#

set -euo pipefail

readonly IMAGE_NAME="thunderos-build:latest"

ensure_docker() {
    if ! command -v docker >/dev/null 2>&1; then
        echo "✗ ERROR: docker not found" >&2
        exit 1
    fi
}

ensure_image() {
    if ! docker image inspect "${IMAGE_NAME}" >/dev/null 2>&1; then
        echo "Docker image ${IMAGE_NAME} not found, building..."
        docker build -t "${IMAGE_NAME}" .
    fi
}

main() {
    local docker_args

    ensure_docker
    ensure_image

    docker_args=(--rm -v "$PWD:/workspace" -w /workspace)
    if [[ -t 0 && -t 1 ]]; then
        docker_args=(-it "${docker_args[@]}")
    else
        docker_args=(-i "${docker_args[@]}")
    fi

    exec docker run "${docker_args[@]}" "${IMAGE_NAME}" bash -lc './run_os.sh'
}

main "$@"