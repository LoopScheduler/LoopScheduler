cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 20)

project(LoopScheduler)

if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

endif()

option(BUILD_TESTS "Build the tests" OFF)

add_subdirectory(LoopScheduler)

if (BUILD_TESTS)
  add_subdirectory(Tests)
endif()
