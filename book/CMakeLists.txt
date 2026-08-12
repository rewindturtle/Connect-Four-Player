cmake_minimum_required(VERSION 3.15)
project(idle_search CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_executable(idle_search main.cpp)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(idle_search PRIVATE -O3 -Wall -Wextra)
endif()

# Portable binary on Windows; MinGW runtime DLLs are not always on PATH
if(MINGW)
    target_link_options(idle_search PRIVATE -static)
endif()
