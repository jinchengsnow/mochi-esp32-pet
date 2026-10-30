#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build-host
clang -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -I main main/pet_model.c main/pet_routine.c tests/test_model.c -lm -o build-host/test_model
build-host/test_model
clang -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -I main main/pet_model.c main/pet_routine.c main/pet_save.c tests/test_save.c -lm -o build-host/test_save
build-host/test_save
clang -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -I main main/pet_battery.c tests/test_battery.c -o build-host/test_battery
build-host/test_battery
clang -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -I main main/pet_power_key.c tests/test_power_key.c -o build-host/test_power_key
build-host/test_power_key
clang -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -I main main/pet_model.c main/pet_routine.c tests/test_time.c -lm -o build-host/test_time
build-host/test_time
clang -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -I main main/pet_model.c main/pet_routine.c main/pet_touch.c tests/test_touch.c -lm -o build-host/test_touch
build-host/test_touch
clang -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -I main main/pet_model.c main/pet_routine.c tests/test_routine.c -lm -o build-host/test_routine
build-host/test_routine
clang -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -I main main/pet_boot_key.c tests/test_boot_key.c -o build-host/test_boot_key
build-host/test_boot_key
clang -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -I main main/pet_sound.c tests/test_sound.c -lm -o build-host/test_sound
build-host/test_sound
