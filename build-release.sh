#!/usr/bin/env bash
set -Eeuo pipefail

readonly REPO_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly VERSION="${1:?Usage: build-release.sh VERSION}"

if [[ ! "$VERSION" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    printf 'Version must look like v1.0.0\n' >&2
    exit 2
fi

cd "$REPO_DIR"
install -d dist
archive="dist/gsr-focused-audio-recorder-${VERSION}-linux-x86_64.tar.gz"

tar --sort=name --mtime='@0' --owner=0 --group=0 --numeric-owner \
    --transform="s,^,gsr-focused-audio-recorder-${VERSION}/," \
    -cf - LICENSE README.md install.sh payload \
    src/gsr-focused-tray/CMakeLists.txt \
    src/gsr-focused-tray/Containerfile \
    src/gsr-focused-tray/build-container.sh \
    src/gsr-focused-tray/main.cpp \
    src/gsr-focused-tray/share-youtube.cpp \
    src/purpose-youtube/README.md \
    src/purpose-youtube/purpose.patch \
    src/purpose-youtube/youtubetray.cpp | gzip -n > "$archive"

(
    cd dist
    sha256sum "${archive##*/}" > "${archive##*/}.sha256"
)
printf 'Created %s and %s.sha256\n' "$archive" "$archive"
