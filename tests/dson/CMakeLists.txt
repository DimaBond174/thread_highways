set(EXE_NAME  "test_dson")

file(GLOB_RECURSE EXE_SRC
       ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
   )

enable_testing()

add_executable(${EXE_NAME}
  ${EXE_SRC}
)

find_package(Threads REQUIRED)

target_link_libraries(${EXE_NAME}
  PRIVATE
  gtest_main
  thread_highways  
  ${CMAKE_THREAD_LIBS_INIT}
)

target_include_directories(${EXE_NAME}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# See how to add googletest to project
# https://google.github.io/googletest/quickstart-cmake.html
include(GoogleTest)
gtest_discover_tests(test_dson)
