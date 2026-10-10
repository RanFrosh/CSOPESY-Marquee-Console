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
  "stb_image.h"

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

  # Backends: OpenGl with GLFW
	"${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
	"${IMGUI_DIR}/backends/imgui_impl_opengl3.h"
	"${IMGUI_DIR}/backends/imgui_impl_opengl3_loader.h"
	"${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
	"${IMGUI_DIR}/backends/imgui_impl_glfw.h"
)

target_include_directories (M04 PRIVATE "${IMGUI_DIR}" "${IMGUI_DIR}/backends" "${IMGUI_DIR}/examples/libs/glfw/include")

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/rick.png" RICK_PNG_PATH)
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bee.png" BEE_IMG_PATH)
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bee.txt" BEE_TXT_PATH)
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/googol.png" GOOGOL_PNG_PATH)
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/mgr.png" MGR_PNG_PATH)

target_compile_definitions(M04 PRIVATE RICK_PNG_PATH="${RICK_PNG_PATH}" BEE_IMG_PATH="${BEE_IMG_PATH}" BEE_TXT_PATH="${BEE_TXT_PATH}" GOOGOL_PNG_PATH="${GOOGOL_PNG_PATH}" MGR_PNG_PATH="${MGR_PNG_PATH}")

# OpenGL3 with GLFW libraries.
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(GLFW_LIB "${IMGUI_DIR}/examples/libs/glfw/lib-vc2010-64/glfw3.lib")
else()
  set(GLFW_LIB "${IMGUI_DIR}/examples/libs/glfw/lib-vc2010-32/glfw3.lib")
endif()
target_link_libraries (M04 PRIVATE opengl32 "${GLFW_LIB}")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET M04 PROPERTY CXX_STANDARD 20)
  set_property(TARGET M04 PROPERTY CXX_STANDARD_REQUIRED ON)
endif()
