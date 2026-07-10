cmake_minimum_required(VERSION 3.18)
project(react-native-blur-android LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BLUR_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../blur-core")

add_subdirectory("${BLUR_CORE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/blur-core")

add_library(blur-jni SHARED
    src/main/cpp/JNIBridge.cpp
)

target_include_directories(blur-jni PRIVATE
    "${BLUR_CORE_DIR}/include"
)

target_compile_options(blur-jni PRIVATE
    -Wall
    -Wextra
    -O2
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(blur-jni PRIVATE -g -O0)
    target_compile_definitions(blur-jni PRIVATE DEBUG=1)
endif()

find_library(jnigraphics-lib jnigraphics)
find_library(log-lib log)

target_link_libraries(blur-jni
    blur-core
    ${jnigraphics-lib}
    ${log-lib}
)
