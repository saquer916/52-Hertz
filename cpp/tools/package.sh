#!/usr/bin/env bash
# Builds shareable zips for itch.io:
#   dist/52-Hertz-linux.zip    (built here; runs on up-to-date Linux)
#   dist/52-Hertz-windows.zip  (cross-compiled with Zig: `sudo pacman -S zig`)
# Each has the game, the whale model, the README and the licenses.
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT=$(pwd)
rm -rf dist && mkdir -p dist

stage() {  # stage <name> <program file>
  local dir="dist/$1"
  mkdir -p "$dir/assets"
  cp "$2" "$dir/"
  cp -r ../assets/models/humpback_whale "$dir/assets/"
  find "$dir/assets" -name '*.import' -delete
  cp README.md "$dir/README.md"
  cp ../LICENSE "$dir/LICENSE.txt"
  (cd "$dir" && bsdtar -a -cf "$ROOT/dist/$1.zip" .)
  echo "made dist/$1.zip ($(du -h "dist/$1.zip" | cut -f1))"
}

echo "== Linux =="
cmake -S . -B build-linux -DPORTABLE=ON -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build build-linux -j"$(nproc)"
stage 52-Hertz-linux build-linux/52hertz

echo "== Windows =="
cmake -S . -B build-windows -DPORTABLE=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT/tools/windows-toolchain.cmake" > /dev/null
cmake --build build-windows -j"$(nproc)"
stage 52-Hertz-windows build-windows/52hertz.exe
