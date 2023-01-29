set(LIB_NAME  "exceptions_signal_lib")
message(STATUS "building ${LIB_NAME}")

file(GLOB_RECURSE LIB_SRC
       ${CMAKE_CURRENT_SOURCE_DIR}/libsrc/*.cpp
   )

add_library(${LIB_NAME}
      SHARED
      ${LIB_SRC}
      )

target_link_libraries(${LIB_NAME}
    PRIVATE
    thread_highways
  )

# target_compile_definitions(${LIB_NAME} PUBLIC BUILDING_LIB) // in cpp

set(EXE_NAME  "exceptions_signal")
message(STATUS "building ${EXE_NAME}")

file(GLOB_RECURSE EXE_SRC
       ${CMAKE_CURRENT_SOURCE_DIR}/mainsrc/*.cpp
   )
   
add_executable(${EXE_NAME}
  ${EXE_SRC}
)

find_package( Threads )

target_link_libraries(${EXE_NAME}
  PRIVATE
  ${LIB_NAME}
  thread_highways
  ${CMAKE_THREAD_LIBS_INIT}
)

target_include_directories(${EXE_NAME}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/libsrc
    ${CMAKE_CURRENT_SOURCE_DIR}/mainsrc
    HIGHWAYS_INCLUDES
)

