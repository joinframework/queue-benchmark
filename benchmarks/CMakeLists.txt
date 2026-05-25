function(add_queue_bench target source)
    add_executable(${target} ${source})
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/benchmarks)
    target_link_libraries(${target} PRIVATE
        benchmark::benchmark_main
        Boost::lockfree
        join::core
        MPMCQueue::MPMCQueue
        readerwriterqueue
        concurrentqueue
        SPSCQueue::SPSCQueue
        Threads::Threads
    )
    if(folly_FOUND)
        target_link_libraries(${target} PRIVATE Folly::folly)
        target_compile_definitions(${target} PRIVATE HAVE_FOLLY)
    endif()
    target_compile_options(${target} PRIVATE -Wall -Wextra)
endfunction()

add_subdirectory(spsc)
add_subdirectory(mpsc)
add_subdirectory(mpmc)
