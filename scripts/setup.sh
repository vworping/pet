#!/bin/bash
set -euo pipefail
PET_IDF_PATH="${PET_IDF_PATH:-/private/tmp/pet-v4-sdk}"
export IDF_TOOLS_PATH="${PET_IDF_TOOLS_PATH:-/private/tmp/pet-v4-tools}"
if [[ ! -d "$PET_IDF_PATH/.git" ]]; then
    git clone --branch v5.4.2 --depth 1 --shallow-submodules --recursive https://github.com/espressif/esp-idf.git "$PET_IDF_PATH"
fi
PET_PYTHON="${PET_PYTHON:-python3.13}"
if ! command -v "$PET_PYTHON" >/dev/null; then
    echo 'Install Python 3.12 or 3.13, or set PET_PYTHON to its executable.' >&2
    exit 1
fi
"$PET_PYTHON" "$PET_IDF_PATH/tools/idf_tools.py" install --targets esp32s3
"$PET_PYTHON" "$PET_IDF_PATH/tools/idf_tools.py" install-python-env
