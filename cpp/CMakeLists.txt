cmake_minimum_required(VERSION 3.20)
project(fiftytwo_hertz CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(FetchContent)

# GLFW and glm: use the system copies when installed, otherwise fetch them.
find_package(glfw3 3.3 QUIET)
if(NOT glfw3_FOUND)
  message(STATUS "GLFW not found on the system, fetching it")
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4)
  FetchContent_MakeAvailable(glfw)
endif()

find_package(glm QUIET)
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
target_link_libraries(52hertz PRIVATE glfw glm::glm)
target_compile_definitions(52hertz PRIVATE
  GLFW_INCLUDE_NONE
  GLM_FORCE_DEPTH_ZERO_TO_ONE
  ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets/models")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(52hertz PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()
find_package(Threads REQUIRED)
target_link_libraries(52hertz PRIVATE Threads::Threads)
