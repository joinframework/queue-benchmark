include(FetchContent)

FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.8.4
    GIT_SHALLOW    TRUE
)
set(BENCHMARK_ENABLE_TESTING        OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS    OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

FetchContent_Declare(
    readerwriterqueue
    GIT_REPOSITORY https://github.com/cameron314/readerwriterqueue.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(readerwriterqueue)

FetchContent_Declare(
    concurrentqueue
    GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(concurrentqueue)

FetchContent_Declare(
    SPSCQueue
    GIT_REPOSITORY https://github.com/rigtorp/SPSCQueue.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SPSCQueue)

FetchContent_Declare(
    join
    GIT_REPOSITORY https://github.com/joinframework/join.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(join)
if(NOT join_POPULATED)
    FetchContent_Populate(join)
endif()
add_library(join_queue_min STATIC ${join_SOURCE_DIR}/core/src/error.cpp)
add_library(join::core ALIAS join_queue_min)
target_include_directories(join_queue_min
    PUBLIC  ${join_SOURCE_DIR}/core/include
    PRIVATE ${join_SOURCE_DIR}/core/src
)

FetchContent_Declare(
    MPMCQueue
    GIT_REPOSITORY https://github.com/rigtorp/MPMCQueue.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(MPMCQueue)

FetchContent_Declare(
    Boost
    URL      https://github.com/boostorg/boost/releases/download/boost-1.86.0/boost-1.86.0-cmake.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP ON
)
set(BOOST_INCLUDE_LIBRARIES lockfree)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_MakeAvailable(Boost)
