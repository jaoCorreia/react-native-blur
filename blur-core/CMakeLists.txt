cmake_minimum_required(VERSION 3.18...4.0)
project(blur-core VERSION 1.4.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BLUR_ENABLE_NEON "Enable ARM NEON optimizations" ON)
option(BLUR_ENABLE_AVX2 "Enable x86_64 AVX2 code path (dispatched at runtime)" ON)
option(BLUR_BUILD_TESTS "Build unit tests" OFF)
option(BLUR_BUILD_BENCHMARKS "Build performance benchmarks" OFF)

set(BLUR_CORE_SOURCES
    src/kernel.cpp
    src/gaussian.cpp
    src/boxblur.cpp
    src/scale.cpp
    src/cache.cpp
    src/pool.cpp
    src/thread_pool.cpp
)

if(BLUR_ENABLE_NEON AND CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|armv7")
    list(APPEND BLUR_CORE_SOURCES src/neon/gaussian_neon.cpp)
endif()

set(BLUR_HAS_AVX2_SOURCE FALSE)
if(BLUR_ENABLE_AVX2 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    list(APPEND BLUR_CORE_SOURCES src/avx2/gaussian_avx2.cpp)
    set(BLUR_HAS_AVX2_SOURCE TRUE)
endif()

add_library(blur-core STATIC ${BLUR_CORE_SOURCES})
add_library(blur::blur-core ALIAS blur-core)

if(BLUR_HAS_AVX2_SOURCE)
    # Only this one file targets AVX2; everything else stays on the
    # compiler's default baseline. Whether it's ever executed is gated by a
    # runtime cpuid check (cpuSupportsAVX2), not by this compile flag.
    set_source_files_properties(src/avx2/gaussian_avx2.cpp PROPERTIES
        COMPILE_OPTIONS "-mavx2"
    )
    set_source_files_properties(src/boxblur.cpp PROPERTIES
        COMPILE_OPTIONS "-msse4.1"
    )
endif()

target_include_directories(blur-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(ANDROID)
    find_library(log-lib log)
    target_link_libraries(blur-core PUBLIC ${log-lib})
endif()

# Lets external CMake projects consume this library standalone (outside the
# React Native / Gradle build) via find_package(blur-core).
include(GNUInstallDirs)

install(TARGETS blur-core
    EXPORT blur-core-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT blur-core-targets
    FILE blur-core-targets.cmake
    NAMESPACE blur::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/blur-core
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/blur-core-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/blur-core-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/blur-core
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/blur-core-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/blur-core-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/blur-core-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/blur-core
)

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
        tests/test_boxblur.cpp
        tests/test_cache.cpp
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
