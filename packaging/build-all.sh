#!/bin/bash
# Build .deb packages for all 5 schemes on Ubuntu 24.04
#
# Usage: ./build-all.sh [APP_VERSION]
#   APP_VERSION defaults to 2.0.0
#
# Note: This is the Ubuntu 24.04 compat build. The .deb internal Version is
#   suffixed with ~ubuntu24.04 (Debian convention: '~' sorts before '.', i.e.
#   this is a "downgraded variant" of APP_VERSION targeting fcitx5 5.1.7).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="${PROJECT_DIR}/build/deb"
APP_VERSION="${1:-2.0.0}"
SCHEMES_CONF="${SCRIPT_DIR}/schemes.conf"
UBUNTU_VER="24.04"
DEB_VERSION="${APP_VERSION}~ubuntu${UBUNTU_VER}"

mkdir -p "$OUTPUT_DIR"

echo "=== Building .deb packages for all schemes (Ubuntu ${UBUNTU_VER}) ==="
echo "Project:     $PROJECT_DIR"
echo "Version:     $APP_VERSION"
echo "Deb Version: $DEB_VERSION"
echo "Ubuntu:      $UBUNTU_VER"
echo "Output:      $OUTPUT_DIR"
echo ""

# Read schemes.conf, skip comments and empty lines
while IFS=$'\t ' read -r SUBMODULE ID NAME LABEL LANG_CODE ICON_TOO rest; do
    # Skip comments and empty lines
    [[ -z "$SUBMODULE" || "$SUBMODULE" == \#* ]] && continue

    # Convert underscores to spaces in SCHEME_NAME
    NAME="${NAME//_/ }"

    echo "--- Building: $NAME ($ID) ---"

    DOCKERFILE="${SCRIPT_DIR}/Dockerfile-${UBUNTU_VER}"
    TAG="fcitx5-${ID}-ubuntu${UBUNTU_VER}"
    DEB_FILE="fcitx5-${ID}_${APP_VERSION}_ubuntu${UBUNTU_VER}.deb"

    docker build \
        -f "$DOCKERFILE" \
        --build-arg "SCHEME_SUBMODULE=$SUBMODULE" \
        --build-arg "SCHEME_ID=$ID" \
        --build-arg "SCHEME_NAME=$NAME" \
        --build-arg "SCHEME_LABEL=$LABEL" \
        --build-arg "SCHEME_LANG_CODE=$LANG_CODE" \
        --build-arg "SCHEME_ICON_TOO=$ICON_TOO" \
        --build-arg "APP_VERSION=$DEB_VERSION" \
        -t "$TAG" \
        "$PROJECT_DIR"

    # Extract .deb from container
    docker run --rm "$TAG" > "$OUTPUT_DIR/$DEB_FILE"

    echo "  Done: $OUTPUT_DIR/$DEB_FILE"
    echo ""
done < "$SCHEMES_CONF"

echo "=== All packages built ==="
echo "Output directory: $OUTPUT_DIR"
ls -la "$OUTPUT_DIR/"*.deb 2>/dev/null || echo "No .deb files found"
