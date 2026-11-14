#!/usr/bin/env bash
# Builds shareable zips for itch.io:
#   dist/52-Hertz-linux.zip    (built here; runs on up-to-date Linux)
#   dist/52-Hertz-windows.zip  (cross-compiled with Zig: `sudo pacman -S zig`)
#   dist/52-Hertz-web.zip      (Emscripten: `sudo pacman -S emscripten`; upload as "played in the browser")
#   dist/52-Hertz-macos.zip    (downloaded from the latest GitHub Actions "Build macOS" run, if `gh` can)
# The download zips have the game, the whale model, the README and the licenses.
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

echo "== Web =="
export PATH="/usr/lib/emscripten:$PATH"
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build build-web -j"$(nproc)"
rm -rf dist/web && mkdir -p dist/web
cp build-web/52hertz.html dist/web/index.html  # itch.io opens index.html
cp build-web/52hertz.js build-web/52hertz.wasm build-web/52hertz.data dist/web/
(cd dist/web && bsdtar -a -cf "$ROOT/dist/52-Hertz-web.zip" .)
echo "made dist/52-Hertz-web.zip ($(du -h dist/52-Hertz-web.zip | cut -f1))"

echo "== macOS (from GitHub Actions) =="
if run=$(gh run list --workflow build-macos.yml --status success --limit 1 --json databaseId --jq '.[0].databaseId' 2>/dev/null) && [ -n "$run" ]; then
  rm -rf dist/52-Hertz-macos && gh run download "$run" --name 52-Hertz-macos --dir dist/52-Hertz-macos
  chmod +x dist/52-Hertz-macos/52hertz
  (cd dist/52-Hertz-macos && bsdtar -a -cf "$ROOT/dist/52-Hertz-macos.zip" .)
  echo "made dist/52-Hertz-macos.zip from run $run"
else
  echo "(skipped: no successful macOS build found)"
fi
