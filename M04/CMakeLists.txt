# CMakeList.txt : CMake project for M04, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT ProgramDatabase)
endif()

project ("M04")

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui")

# Add source to this project's executable.
add_executable (M04
  "M04.cpp"
  "M04.h"
  "${IMGUI_DIR}/imgui.cpp"
  "${IMGUI_DIR}/imgui_demo.cpp"
  "${IMGUI_DIR}/imgui_draw.cpp"
  "${IMGUI_DIR}/imgui_tables.cpp"
  "${IMGUI_DIR}/imgui_widgets.cpp"
)

target_include_directories (M04 PRIVATE "${IMGUI_DIR}")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET M04 PROPERTY CXX_STANDARD 20)
  set_property(TARGET M04 PROPERTY CXX_STANDARD_REQUIRED ON)
endif()
