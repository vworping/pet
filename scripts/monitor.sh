#!/bin/bash
set -eo pipefail
source "$(dirname "$0")/env.sh"
PET_PORT="${PET_PORT:-$(python "$PET_ROOT/scripts/port.py")}"
cd "$PET_BUILD_ROOT"
idf.py -p "$PET_PORT" monitor
