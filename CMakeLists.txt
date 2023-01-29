cmake_minimum_required(VERSION 3.19 FATAL_ERROR)
project(thread_highways)

set(CMAKE_CXX_STANDARD 17)

set(LIB_NAME thread_highways)

add_library(${LIB_NAME} INTERFACE)

target_include_directories(${LIB_NAME}
  INTERFACE
  include/
)

set (HIGHWAYS_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include/ )

#set_target_properties(${LIB_NAME} PROPERTIES
#    CXX_STANDARD 17
#    CXX_STANDARD_REQUIRED ON
#)

option(BUILD_EXAMPLES "Build examples " ON)
option(BUILD_PERFORMANCE_TESTS  "Build performance tests " ON)
option(BUILD_TESTING "Build unit tests" ON)

if(BUILD_EXAMPLES)
    message(STATUS "building thread_highways examples..")
    add_subdirectory(examples)
endif()

#if(BUILD_PERFORMANCE_TESTS)
#    message(STATUS "building performance tests..")
#    add_subdirectory(performance_tests)
#endif()

#if(BUILD_TESTING)
#    message(STATUS "building googletest..")
#    # https://google.github.io/googletest/quickstart-cmake.html
#    include(FetchContent)
#    FetchContent_Declare(
#      googletest
#      URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
#    )
#    # For Windows: Prevent overriding the parent project's compiler/linker settings
#    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
#    FetchContent_MakeAvailable(googletest)

#    message(STATUS "building thread_highways unit tests..")
#    add_subdirectory(tests)
#endif()

