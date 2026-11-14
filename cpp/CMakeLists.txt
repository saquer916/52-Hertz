cmake_minimum_required(VERSION 3.20)
project(fiftytwo_hertz CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(FetchContent)

# PORTABLE=ON makes a self-contained build to share (tools/package.sh uses it):
# GLFW is built into the game, and assets are loaded from next to the program.
option(PORTABLE "Self-contained build for sharing" OFF)
if(PORTABLE)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
endif()

# GLFW and glm: use the system copies when installed, otherwise fetch them.
# (In the browser, Emscripten provides GLFW itself.)
if(NOT PORTABLE AND NOT EMSCRIPTEN)
  find_package(glfw3 3.3 QUIET)
endif()
if(NOT glfw3_FOUND AND NOT EMSCRIPTEN)
  message(STATUS "GLFW not found on the system, fetching it")
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4)
  FetchContent_MakeAvailable(glfw)
endif()

if(NOT PORTABLE)
  find_package(glm QUIET)
endif()
if(NOT glm_FOUND)
  message(STATUS "glm not found on the system, fetching it")
  FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG 1.0.1)
  FetchContent_MakeAvailable(glm)
endif()

add_executable(52hertz
  src/main.cpp
  src/game.cpp
  src/gameplay.cpp
  src/gl.cpp
  src/render_util.cpp
  src/whale_model.cpp
  src/level.cpp
  src/ocean.cpp
  src/echo.cpp
  src/effects.cpp
  src/hud.cpp
  src/post.cpp
  src/third_party_impl.cpp
)
target_include_directories(52hertz PRIVATE src third_party)
if(EMSCRIPTEN)
  target_link_libraries(52hertz PRIVATE glm::glm)
else()
  target_link_libraries(52hertz PRIVATE glfw glm::glm)
endif()
target_compile_definitions(52hertz PRIVATE
  GLFW_INCLUDE_NONE
  GLM_FORCE_DEPTH_ZERO_TO_ONE)
# macOS only has OpenGL 4.1. GL41=ON uses that path anywhere, to test it on Linux/Windows.
option(GL41 "Use the OpenGL 4.1 code path (always on for macOS)" OFF)
# Browsers have WebGL 2 (GLSL ES 3.00). ESSL=ON uses those shaders on desktop, to test them.
option(ESSL "Use WebGL-style GLSL ES 3.00 shaders (always on for the web build)" OFF)
if(APPLE OR GL41 OR ESSL OR EMSCRIPTEN)
  target_compile_definitions(52hertz PRIVATE HERTZ_GL41)
endif()
if(ESSL OR EMSCRIPTEN)
  target_compile_definitions(52hertz PRIVATE HERTZ_ES)
endif()
if(EMSCRIPTEN)
  # Browser build: 52hertz.html + .js + .wasm + .data (the whale, preloaded into memory).
  set_target_properties(52hertz PROPERTIES SUFFIX ".html")
  target_compile_definitions(52hertz PRIVATE ASSET_DIR="assets")
  target_link_options(52hertz PRIVATE
    -sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3=1
    -sGL_ENABLE_GET_PROC_ADDRESS=1 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=256MB
    --profiling-funcs -sASSERTIONS=1
    --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/../assets/models/humpback_whale@/assets/humpback_whale
    --exclude-file *.import
    --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html)
elseif(PORTABLE)
  target_compile_definitions(52hertz PRIVATE ASSET_DIR="assets" PORTABLE_ASSETS)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(52hertz PRIVATE -static-libstdc++ -static-libgcc)
  endif()
else()
  target_compile_definitions(52hertz PRIVATE ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets/models")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(52hertz PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()
if(NOT EMSCRIPTEN)  # the browser build has no threads
  find_package(Threads REQUIRED)
  target_link_libraries(52hertz PRIVATE Threads::Threads)
endif()
