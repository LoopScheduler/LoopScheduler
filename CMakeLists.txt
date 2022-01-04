cmake_minimum_required(VERSION 3.12)

project(LoopScheduler)

set(CMAKE_CXX_STANDARD 20)

if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

endif()

option(LOOPSCHEDULER_BUILD_TESTS "Build the tests" OFF)

add_subdirectory(LoopScheduler)

if (LOOPSCHEDULER_BUILD_TESTS)
  add_subdirectory(Tests)
endif()
