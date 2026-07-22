cmake_minimum_required(VERSION 3.16)
project(packet_filter_vm CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Warnings on from day one — treat them as the first code review you get.
add_compile_options(-Wall -Wextra -Wpedantic)

# Core library: compiler + VM (no graphics deps — keep it that way so the
# CLI and tests never need GLFW/OpenGL installed).
add_library(pfc_core
    src/lexer.cpp
    src/parser.cpp
    src/codegen.cpp
    src/vm.cpp
    src/bytecode.cpp
)
target_include_directories(pfc_core PUBLIC include)

# Headless CLI driver
add_executable(pfc src/main.cpp)
target_link_libraries(pfc PRIVATE pfc_core)

# Test suite
add_executable(run_tests tests/run_tests.cpp)
target_link_libraries(run_tests PRIVATE pfc_core)

# ---------------------------------------------------------------------------
# Stage 6 (uncomment when you start the visualizer):
#
#   brew install glfw
#
# find_package(glfw3 REQUIRED)
# find_package(OpenGL REQUIRED)
# add_executable(pfc_viz src/viz/main_viz.cpp src/viz/renderer.cpp)
# target_link_libraries(pfc_viz PRIVATE pfc_core glfw OpenGL::GL)
# if(APPLE)
#   target_compile_definitions(pfc_viz PRIVATE GL_SILENCE_DEPRECATION)
# endif()
# ---------------------------------------------------------------------------
