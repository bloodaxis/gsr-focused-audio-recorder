#!/usr/bin/env bash
set -Eeuo pipefail

readonly SOURCE_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly OUTPUT_DIR="$SOURCE_DIR/build-output"
readonly IMAGE="localhost/gsr-focused-tray-builder:fedora44"
readonly CONTAINER="gsr-focused-tray-builder"

mkdir -p -- "$OUTPUT_DIR"

if ! podman image exists "$IMAGE"; then
    podman build -t "$IMAGE" -f "$SOURCE_DIR/Containerfile" "$SOURCE_DIR"
fi

if ! podman container exists "$CONTAINER"; then
    podman create \
        --name "$CONTAINER" \
        -v "$SOURCE_DIR:/src:ro,Z" \
        -v "$OUTPUT_DIR:/out:Z" \
        "$IMAGE" >/dev/null
fi

podman start --attach "$CONTAINER"
printf 'Built helpers in %s\n' "$OUTPUT_DIR"
