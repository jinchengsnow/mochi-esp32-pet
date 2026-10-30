#!/bin/bash
set -euo pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
pet_tools="${PET_BUILD_TOOLS:-/Users/jincheng/esp/tools-venv/bin}"
export PATH="$pet_tools:$PATH"
pet_python="${PET_PREVIEW_PYTHON:-/Users/jincheng/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3}"
cd "$project_dir"
if [ ! -d managed_components/lvgl__lvgl ]; then
    echo "Run bash scripts/build.sh first to resolve LVGL 9.5.0." >&2
    exit 1
fi
cmake -S preview -B build-host/renderer -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-host/renderer -j 8
# A new private folder makes retries independent of earlier captures.
pet_frames="$(mktemp -d "$project_dir/build-host/preview-XXXXXX")"
trap 'rm -rf "$pet_frames"' EXIT
build-host/renderer/pet_preview "$pet_frames/frames"
"$pet_python" preview/package_preview.py "$pet_frames/frames"
