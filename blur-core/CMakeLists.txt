cmake_minimum_required(VERSION 3.18...4.0)
project(blur-core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BLUR_ENABLE_NEON "Enable ARM NEON optimizations" ON)
option(BLUR_BUILD_TESTS "Build unit tests" OFF)
option(BLUR_BUILD_BENCHMARKS "Build performance benchmarks" OFF)

set(BLUR_CORE_SOURCES
    src/kernel.cpp
    src/gaussian.cpp
    src/scale.cpp
    src/cache.cpp
    src/thread_pool.cpp
)

if(BLUR_ENABLE_NEON AND CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|armv7")
    list(APPEND BLUR_CORE_SOURCES src/neon/gaussian_neon.cpp)
endif()

add_library(blur-core STATIC ${BLUR_CORE_SOURCES})

target_include_directories(blur-core PUBLIC include)

if(ANDROID)
    find_library(log-lib log)
    target_link_libraries(blur-core PUBLIC ${log-lib})
endif()

if(BLUR_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(blur-tests
        tests/test_kernel.cpp
        tests/test_gaussian.cpp
        tests/test_main.cpp
    )

    target_link_libraries(blur-tests PRIVATE blur-core gtest gtest_main)
    target_include_directories(blur-tests PRIVATE include)

    include(GoogleTest)
    gtest_discover_tests(blur-tests)
endif()

if(BLUR_BUILD_BENCHMARKS)
    include(FetchContent)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.1
    )
    FetchContent_MakeAvailable(googlebenchmark)

    add_executable(blur-bench
        tests/benchmark_blur.cpp
    )

    target_link_libraries(blur-bench PRIVATE blur-core benchmark::benchmark benchmark::benchmark_main)
    target_include_directories(blur-bench PRIVATE include)
endif()
