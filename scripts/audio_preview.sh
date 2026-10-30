#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build-host docs/audio
clang -std=c11 -Wall -Wextra -Werror -I main main/pet_sound.c preview/sounds.c -lm -o build-host/pet_sounds
build-host/pet_sounds docs/audio
