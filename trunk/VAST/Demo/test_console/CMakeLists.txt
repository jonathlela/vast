include_directories("${CMAKE_SOURCE_DIR}/common" "${CMAKE_SOURCE_DIR}/VASTsim" "${ACE_INCLUDE_DIR}")

add_definitions("-Wall -g")

add_executable (test_console test_console.cpp) 

link_directories(${CMAKE_SOURCE_DIR}/lib)

target_link_libraries(test_console
  ${ACE_LIBRARY}
  ${PTHREAD_LIBRARY}
  ${LIBDL_LIBRARY}
 ${ZLIB_LIBRARY}
 vast
 vastsim
 vastnet
 vastcommon
)

