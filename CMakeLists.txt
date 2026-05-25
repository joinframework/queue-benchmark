cmake_minimum_required(VERSION 3.20)
project(queue-bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-march=native)

find_package(Threads REQUIRED)
find_package(folly QUIET)
if(folly_FOUND)
    message(STATUS "Folly found — Folly MPMC benchmarks enabled")
else()
    message(STATUS "Folly not found — install libfolly-dev to enable Folly benchmarks")
endif()
include(cmake/deps.cmake)
add_subdirectory(benchmarks)
