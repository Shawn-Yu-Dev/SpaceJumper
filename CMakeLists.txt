cmake_minimum_required(VERSION 3.10)
project(SuperPlatformer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ==========================================================================
# SDL2 发现 (跨平台双通道: CMake config → pkg-config 兜底)
#   Windows (vcpkg):    find_package(SDL2 CONFIG)  → SDL2::SDL2 + SDL2::SDL2main
#   Windows (MinGW):    pkg-config sdl2             → PkgConfig::SDL2 (含 -lmingw32 -lSDL2main)
#   Linux / macOS:      find_package 或 pkg-config
# ==========================================================================
find_package(SDL2 CONFIG QUIET)
if(SDL2_FOUND)
  set(SDL2_LIB_TARGET SDL2::SDL2)
  if(WIN32 AND TARGET SDL2::SDL2main)
    set(SDL2_MAIN_TARGET SDL2::SDL2main)
  endif()
  message(STATUS "SDL2 found via CMake config (vcpkg / official package)")
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  set(SDL2_LIB_TARGET PkgConfig::SDL2)
  message(STATUS "SDL2 found via pkg-config ${SDL2_VERSION}")
endif()

# ── 源文件 (按模块组织) ──────────────────────────────────────────
add_executable(mario
  src/main.cpp
  src/core/Common.cpp
  src/core/GameLogic.cpp
  src/core/Physics.cpp
  src/level/Level.cpp
  src/render/Renderer.cpp
  src/render/Font.cpp
  src/audio/Audio.cpp
  src/save/Save.cpp
)

# 所有模块子目录加入 include 搜索路径, #include "X.hpp" 无需改路径
target_include_directories(mario PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core
  ${CMAKE_CURRENT_SOURCE_DIR}/src/level
  ${CMAKE_CURRENT_SOURCE_DIR}/src/render
  ${CMAKE_CURRENT_SOURCE_DIR}/src/audio
  ${CMAKE_CURRENT_SOURCE_DIR}/src/save
)

target_link_libraries(mario PRIVATE ${SDL2_LIB_TARGET})
if(SDL2_MAIN_TARGET)
  target_link_libraries(mario PRIVATE ${SDL2_MAIN_TARGET})
endif()

# ── 极致优化 (按编译器分派, 避免 MSVC 吞 GCC 专属标志) ────────────
target_compile_options(mario PRIVATE
  # GCC / Clang / AppleClang
  $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-O3>
  $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-march=native>
  # MSVC (O2 已是 Release 默认, 追加向量化与内联激进)
  $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/O2>
  $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/fp:fast>
)

# 无头自检: xvfb-run ./mario --smoke (或 SDL_VIDEODRIVER=dummy)