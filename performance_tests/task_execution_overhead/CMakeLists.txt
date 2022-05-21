set(EXE_NAME  "task_execution_overhead")
message(STATUS "building ${EXE_NAME}")

file(GLOB_RECURSE EXE_SRC
       ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
   )
   
add_executable(${EXE_NAME}
  ${EXE_SRC}
)

find_package( Threads )

target_link_libraries(${EXE_NAME}
  PRIVATE
  thread_highways
  ${CMAKE_THREAD_LIBS_INIT}
)

target_include_directories(${EXE_NAME}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

