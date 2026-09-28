cmake_minimum_required(VERSION 3.16)
project(cpp-intro-site LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# 生成器本体 + 页面素材（HTML / CSS / JS 都是 .cpp 里的字符串）
add_executable(ssg
  src/main.cpp
  src/markdown.cpp
  src/template.cpp
  src/web_page.cpp
  src/web_style.cpp
  src/web_script.cpp
  src/web_fx.cpp
)

target_include_directories(ssg PRIVATE src)

if(MSVC)
  target_compile_options(ssg PRIVATE /utf-8 /W3 /bigobj)
else()
  target_compile_options(ssg PRIVATE -Wall -Wextra)
endif()

# GCC 8 及更早版本需要显式链接 filesystem
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
  target_link_libraries(ssg PRIVATE stdc++fs)
endif()
