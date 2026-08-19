#!/usr/bin/env bash
#
# Fetches the Slang shader-compiler toolchain into Tools/slang (gitignored).
# Slang is the single shader-authoring language for Donut; slangc cross-compiles
# each Assets/Shaders/*.slang to GLSL (OpenGL), SPIR-V (Vulkan), and MSL (Metal).
#
# Re-run to (re)install. Pin the version here so builds are reproducible.
set -euo pipefail

VERSION="2026.14.1"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/Tools/slang"

case "$(uname -s)-$(uname -m)" in
  Darwin-arm64|Darwin-aarch64) ASSET="macos-aarch64" ;;
  Darwin-x86_64)               ASSET="macos-x86_64" ;;
  Linux-x86_64)                ASSET="linux-x86_64" ;;
  Linux-aarch64|Linux-arm64)   ASSET="linux-aarch64" ;;
  *) echo "fetch-slang: unsupported platform $(uname -s)-$(uname -m)"; exit 1 ;;
esac

URL="https://github.com/shader-slang/slang/releases/download/v${VERSION}/slang-${VERSION}-${ASSET}.tar.gz"

echo "fetch-slang: downloading Slang ${VERSION} (${ASSET})..."
rm -rf "$DEST"
mkdir -p "$DEST"
curl -fsSL "$URL" | tar -xz -C "$DEST"

"$DEST/bin/slangc" -v >/dev/null 2>&1 && echo "fetch-slang: installed to $DEST (slangc $("$DEST/bin/slangc" -v))"
