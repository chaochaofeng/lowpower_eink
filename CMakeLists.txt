# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

#set(EXTRA_COMPONENT_DIRS  "components/lvgl")

project(esp_lowpower_eink)

message(STATUS ${PROJECT_DIR})
message(STATUS ${COMPONENT_DIR})
message(STATUS "*************************")