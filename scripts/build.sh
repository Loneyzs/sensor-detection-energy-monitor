#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
zephyr_python="${ZEPHYR_PYTHON:-$HOME/anaconda3/envs/zephyr/bin/python}"
export ZEPHYR_BASE="${ZEPHYR_BASE:-$HOME/zephyrws/zephyr}"
export ZEPHYR_SDK_INSTALL_DIR="${ZEPHYR_SDK_INSTALL_DIR:-$HOME/zephyr_sdk}"
export PATH="$(dirname -- "$zephyr_python"):$PATH"
energy_build_dir="${ENERGY_BUILD_DIR:-$project_dir/build-mspm0}"
exec "$zephyr_python" -m west build -b energy_mspm0g3507 -d "$energy_build_dir" "$project_dir" "$@"
