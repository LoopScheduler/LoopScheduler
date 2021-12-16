cmake_minimum_required(VERSION 3.10.2)

if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -pthread")

endif()

add_subdirectory(LoopScheduler)
add_subdirectory(Tests)
