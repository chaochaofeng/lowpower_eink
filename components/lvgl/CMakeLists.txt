set(LVGL_ROOT_DIR ".")

file(GLOB_RECURSE SOURCES ${LVGL_ROOT_DIR}/src/*.c ${LVGL_ROOT_DIR}/demos/*.c)

idf_component_register(
  SRCS ${SOURCES}
  INCLUDE_DIRS ${LVGL_ROOT_DIR} ${LVGL_ROOT_DIR}/src
	${LVGL_ROOT_DIR}/../
	${LVGL_ROOT_DIR}/examples
	${LVGL_ROOT_DIR}/demos
  REQUIRES esp_timer)