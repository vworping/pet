#!/bin/bash
set -euo pipefail
PET_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PET_STAGE="${PET_BUILD_ROOT:-/private/tmp/pet-v4-build}"
cmake -S "$PET_ROOT/tests/native" -B "$PET_ROOT/.local/native-build" -DLVGL_DIR="$PET_STAGE/managed_components/lvgl__lvgl" -DPET_MBEDTLS_INCLUDE="${PET_IDF_PATH:-/private/tmp/pet-v4-sdk}/components/mbedtls/mbedtls/include"
cmake --build "$PET_ROOT/.local/native-build" -j 8
cd "$PET_ROOT/.local/native-build"
./pet_native
./pet_protocol_test
./pet_socket_test
./pet_enterprise_test
./pet_enterprise_tls_test
