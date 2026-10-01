cmake_minimum_required(VERSION 3.10)
project(SuperPlatformer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 优化: 与单文件版本一致的极致性能 (-O2, 无 RTTI/异常开销之外的改动)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

add_executable(mario
  main.cpp
  Common.cpp
  Font.cpp
  Audio.cpp
  Physics.cpp
  Level.cpp
  Renderer.cpp
  GameLogic.cpp
  Save.cpp
)

target_include_directories(mario PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${SDL2_INCLUDE_DIRS})
target_link_libraries(mario PRIVATE ${SDL2_LIBRARIES})
target_compile_options(mario PRIVATE ${SDL2_CFLAGS_OTHER})

# 无头自检: xvfb-run ./mario --smoke (或 SDL_VIDEODRIVER=dummy)
