#!/bin/bash
set -eo pipefail
source "$(dirname "$0")/env.sh"
mkdir -p "$PET_BUILD_ROOT" "$PET_ROOT/.local"
# Stage sources because the repository's parent directory contains spaces.
# Build products live only in this task-owned directory.
rsync -a --exclude build --exclude managed_components --exclude sdkconfig --exclude sdkconfig.old "$PET_ROOT/firmware/" "$PET_BUILD_ROOT/"
cd "$PET_BUILD_ROOT"
idf.py build
cp dependencies.lock "$PET_ROOT/firmware/dependencies.lock"
