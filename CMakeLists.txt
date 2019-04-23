cmake_minimum_required(VERSION 3.11)
project(multithreading-tests DESCRIPTION "A collection of tests on concurrent implementations of common algorithms." VERSION 1.0.0 LANGUAGES CXX)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/filter")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/merge-sort")
