# CMakeList.txt : CMake project for MO1, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)

# Add source to this project's executable.
add_executable (MO1 "MO1.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET MO1 PROPERTY CXX_STANDARD 20)
endif()

# The scheduler runs on its own thread
find_package(Threads REQUIRED)
target_link_libraries(MO1 PRIVATE Threads::Threads)

# "initialize" reads config.txt from the working directory, so keep a copy next to the executable
add_custom_command(TARGET MO1 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/config.txt"
          "$<TARGET_FILE_DIR:MO1>/config.txt")

# TODO: Add tests and install targets if needed.