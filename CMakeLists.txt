cmake_minimum_required(VERSION 3.19 FATAL_ERROR)
project(thread_highways)

set(CMAKE_CXX_STANDARD 17)

set(LIB_NAME thread_highways)

add_library(${LIB_NAME} INTERFACE)

target_include_directories(${LIB_NAME}
  INTERFACE
  include/
)

set_target_properties(${LIB_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

if(BUILD_TESTING)
    message(STATUS "building thread_highways examples..")
    add_subdirectory(examples)

    message(STATUS "building unit tests..")
    add_subdirectory(tests)
endif()

