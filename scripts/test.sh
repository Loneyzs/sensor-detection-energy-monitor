#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$(mktemp -d /tmp/energy-tests.XXXXXX)"
trap 'rm -rf "$test_dir"' EXIT
cd "$project_dir"
flags=(-std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address -g -Isrc)
cc "${flags[@]}" src/monitor.c tests/test_monitor.c -o "$test_dir/monitor"
"$test_dir/monitor"
cc "${flags[@]}" src/monitor.c src/ui_model.c src/ui_render.c src/button.c \
  tests/test_ui.c -o "$test_dir/ui"
"$test_dir/ui"
cc "${flags[@]}" -Itests/stubs src/ssd1306.c tests/test_oled_transport.c -o "$test_dir/oled"
"$test_dir/oled"
cc "${flags[@]}" -Itests/stubs src/ina219.c tests/test_ina219.c -o "$test_dir/ina219"
"$test_dir/ina219"
