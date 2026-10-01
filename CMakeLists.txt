cmake_minimum_required(VERSION 3.10)
project(SuperPlatformer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

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
  ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(mario PRIVATE ${SDL2_LIBRARIES})
target_compile_options(mario PRIVATE ${SDL2_CFLAGS_OTHER})

# ── -O3 极致优化 (不覆盖 CMake 默认 Release 组合, 只追加) ────────
target_compile_options(mario PRIVATE
  $<$<CONFIG:Release>:-O3>
  $<$<CONFIG:Release>:-DNDEBUG>
  $<$<CONFIG:Release>:-march=native>
)

# 无头自检: xvfb-run ./mario --smoke (或 SDL_VIDEODRIVER=dummy)