#!/bin/bash
# Source from project scripts. Keep ESP-IDF paths free of spaces.
PET_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export IDF_TOOLS_PATH="${PET_IDF_TOOLS_PATH:-/private/tmp/pet-v4-tools}"
export IDF_PATH="${PET_IDF_PATH:-/private/tmp/pet-v4-sdk}"
export PET_BUILD_ROOT="${PET_BUILD_ROOT:-/private/tmp/pet-v4-build}"
if [[ ! -f "$IDF_PATH/export.sh" ]]; then
    echo 'ESP-IDF is missing. Run scripts/setup.sh first.' >&2
    return 1
fi
# Avoid unrelated Conda Python/Ninja installations.
export PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:$PATH"
for PET_ENV in "$IDF_TOOLS_PATH"/python_env/idf5.4_py*_env; do
    if [[ -x "$PET_ENV/bin/python" ]]; then
        export IDF_PYTHON_ENV_PATH="$PET_ENV"
        export PATH="$IDF_PYTHON_ENV_PATH/bin:$PATH"
        break
    fi
done
. "$IDF_PATH/export.sh" > /dev/null
