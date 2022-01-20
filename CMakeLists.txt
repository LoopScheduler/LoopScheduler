cmake_minimum_required(VERSION 3.13)

project(LoopScheduler)

set(CMAKE_CXX_STANDARD 20)

option(LOOPSCHEDULER_BUILD_TESTS "Build the tests" OFF)

add_subdirectory(LoopScheduler)

if (LOOPSCHEDULER_BUILD_TESTS)
    add_subdirectory(Tests)
endif()
