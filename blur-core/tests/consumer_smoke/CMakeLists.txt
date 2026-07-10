cmake_minimum_required(VERSION 3.18)
project(blur-core-consumer-smoke LANGUAGES CXX)

find_package(blur-core REQUIRED)

add_executable(consumer-smoke main.cpp)
target_link_libraries(consumer-smoke PRIVATE blur::blur-core)
