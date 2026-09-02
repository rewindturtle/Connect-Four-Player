cmake_minimum_required(VERSION 3.15)
project(idle_search CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(ROBOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../robot)
set(SHARED_SOURCES
    ${ROBOT_DIR}/src/board.cpp
    ${ROBOT_DIR}/src/memo.cpp)

add_executable(idle_search
    main.cpp
    ${SHARED_SOURCES})

add_executable(memo_tests
    tests.cpp
    ${SHARED_SOURCES})

foreach(target idle_search memo_tests)
    target_compile_definitions(${target} PRIVATE C4_DESKTOP)
    target_include_directories(${target} PRIVATE ${ROBOT_DIR}/include)
endforeach()

enable_testing()
add_test(NAME memo_tests COMMAND memo_tests)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    foreach(target idle_search memo_tests)
        target_compile_options(${target} PRIVATE -O3 -Wall -Wextra)
    endforeach()
endif()

# Portable binary on Windows; MinGW runtime DLLs are not always on PATH
if(MINGW)
    foreach(target idle_search memo_tests)
        target_link_options(${target} PRIVATE -static)
    endforeach()
endif()
