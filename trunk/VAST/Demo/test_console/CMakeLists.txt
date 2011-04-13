include_directories("${CMAKE_SOURCE_DIR}/common" "${CMAKE_SOURCE_DIR}/VASTsim" "${ACE_INCLUDE_DIR}")

add_definitions("-Wall -fPIC")

add_executable (test_console test_console.cpp) 

link_directories(${CMAKE_SOURCE_DIR}/lib)

target_link_libraries(test_console
  ${ACE_LIBRARY}
  pthread
  dl
 vast
 vastsim
 vastnet
 vastcommon
)

