#!/usr/bin/env bash
# Exports Linux + Windows builds and zips them for itch.io.
# Usage: tools/build.sh   (needs Godot 4.7.2 export templates installed)
set -euo pipefail
cd "$(dirname "$0")/.."

rm -rf build
mkdir -p build/linux build/windows

godot --headless --export-release "Linux" build/linux/52-Hertz.x86_64
godot --headless --export-release "Windows Desktop" build/windows/52-Hertz.exe

(cd build/linux && bsdtar -a -cf ../52-Hertz-linux.zip .)
(cd build/windows && bsdtar -a -cf ../52-Hertz-windows.zip .)
ls -lh build/*.zip
