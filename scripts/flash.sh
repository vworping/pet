#!/bin/bash
set -eo pipefail
source "$(dirname "$0")/env.sh"
PET_PORT="${PET_PORT:-$(python "$PET_ROOT/scripts/port.py")}"
if [[ ! -f "$PET_BUILD_ROOT/build/pet_badge.bin" ]]; then
    echo 'No firmware built yet. Run scripts/build.sh.' >&2
    exit 1
fi
printf 'Flashing [pet.] to %s\n' "$PET_PORT"
cd "$PET_BUILD_ROOT"
idf.py -p "$PET_PORT" -b 921600 flash
