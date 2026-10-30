#!/bin/bash
set -euo pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
pet_idf_path="${PET_IDF_PATH:-${IDF_PATH:-/Users/jincheng/esp/esp-idf-v5.5.5}}"
if [ ! -f "$pet_idf_path/export.sh" ]; then
    echo "Set PET_IDF_PATH to an ESP-IDF 5.5.x checkout." >&2
    exit 1
fi
source "$pet_idf_path/export.sh" >/dev/null
pet_tools="${PET_BUILD_TOOLS:-/Users/jincheng/esp/tools-venv/bin}"
if [ -d "$pet_tools" ]; then export PATH="$PATH:$pet_tools"; fi
cd "$project_dir"
idf.py "$@" build
