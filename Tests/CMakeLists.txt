if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))

    add_link_options(-pthread)

endif()

add_executable(combined_test combined_test.cpp)
target_link_libraries(combined_test LoopScheduler)

add_executable(parallel_evaluation parallel_evaluation.cpp)
target_link_libraries(parallel_evaluation LoopScheduler)

add_executable(sequential_evaluation sequential_evaluation.cpp)
target_link_libraries(sequential_evaluation LoopScheduler)
