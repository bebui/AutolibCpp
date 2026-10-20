include(FetchContent)
find_package(benchmark 1.8 QUIET)
if(NOT benchmark_FOUND)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.1
        GIT_SHALLOW TRUE
        SYSTEM)
    FetchContent_MakeAvailable(benchmark)
endif()

add_executable(autolib_benchmarks benchmarks.cpp)
target_link_libraries(autolib_benchmarks PRIVATE autolib::autolib benchmark::benchmark_main)
