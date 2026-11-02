board_runner_args(jlink "--device=MSPM0G3507" "--speed=1000")
board_runner_args(openocd "--config=${BOARD_DIR}/support/openocd.cfg")
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
