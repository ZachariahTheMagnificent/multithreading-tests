cmake_minimum_required(VERSION 3.11)
project(multithreading-filter-test VERSION 1.0.0 DESCRIPTION "A test on a concurrent implementation of a filter algorithm." LANGUAGES CXX)

add_executable(filter-test)
target_compile_features(filter-test PRIVATE cxx_std_17)
# target_compile_definitions(filter-test PRIVATE MULTITHREADING)
target_include_directories(filter-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_sources(filter-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source/main.cpp")

add_executable(multithreading-filter-test)
target_compile_features(multithreading-filter-test PRIVATE cxx_std_17)
target_compile_definitions(multithreading-filter-test PRIVATE MULTITHREADING)
target_include_directories(multithreading-filter-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_sources(multithreading-filter-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source/main.cpp")
