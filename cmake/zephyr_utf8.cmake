# Zephyr's toolchain_parse_make_rule uses file(STRINGS) without UTF-8
# encoding. With a Chinese source path it adds truncated directory names
# as dependencies, causing Ninja to reconfigure forever. Keep real files
# and explicitly track our local DTS inputs. The external tree stays intact.
foreach(configure_dir "${CMAKE_CURRENT_SOURCE_DIR}" "${ZEPHYR_BASE}")
  get_property(configure_inputs DIRECTORY "${configure_dir}" PROPERTY CMAKE_CONFIGURE_DEPENDS)
  set(valid_configure_inputs "")
  foreach(input IN LISTS configure_inputs)
    if(EXISTS "${input}" AND NOT IS_DIRECTORY "${input}")
      list(APPEND valid_configure_inputs "${input}")
    endif()
  endforeach()
  list(APPEND valid_configure_inputs
    "${BOARD_DIR}/${BOARD}.dts" ${DTC_OVERLAY_FILE})
  set_property(DIRECTORY "${configure_dir}" PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${valid_configure_inputs})
endforeach()
