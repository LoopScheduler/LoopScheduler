if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))

    add_compile_options(-pthread -O2)

endif()

file(GLOB loopscheduler_srcs *.cpp)
add_library(LoopScheduler ${loopscheduler_srcs})
