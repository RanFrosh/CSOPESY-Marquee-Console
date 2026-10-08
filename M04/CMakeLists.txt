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

  # Dear ImGui core (headers listed for IDE visibility only)
  "${IMGUI_DIR}/imconfig.h"
  "${IMGUI_DIR}/imgui.h"
  "${IMGUI_DIR}/imgui_internal.h"
  "${IMGUI_DIR}/imstb_rectpack.h"
  "${IMGUI_DIR}/imstb_textedit.h"
  "${IMGUI_DIR}/imstb_truetype.h"
  "${IMGUI_DIR}/imgui.cpp"
  "${IMGUI_DIR}/imgui_demo.cpp"
  "${IMGUI_DIR}/imgui_draw.cpp"
  "${IMGUI_DIR}/imgui_tables.cpp"
  "${IMGUI_DIR}/imgui_widgets.cpp"

  # Backends: Win32 platform + DirectX 11 renderer
  "${IMGUI_DIR}/backends/imgui_impl_win32.h"
  "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
  "${IMGUI_DIR}/backends/imgui_impl_dx11.h"
  "${IMGUI_DIR}/backends/imgui_impl_dx11.cpp"
)

target_include_directories (M04 PRIVATE "${IMGUI_DIR}" "${IMGUI_DIR}/backends")

# DirectX 11 + DXGI come from the Windows SDK installed with Visual Studio.
target_link_libraries (M04 PRIVATE d3d11 dxgi d3dcompiler)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET M04 PROPERTY CXX_STANDARD 20)
  set_property(TARGET M04 PROPERTY CXX_STANDARD_REQUIRED ON)
endif()
