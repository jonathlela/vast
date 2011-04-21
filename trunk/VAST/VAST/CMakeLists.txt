include_directories ("${PROJECT_SOURCE_DIR}/common" "${PROJECT_SOURCE_DIR}/VASTnet" "${ACE_INCLUDE_DIR}")

add_definitions("-Wall -g")

add_library (vast VASTClient.cpp  VASTMatcher.cpp  VASTRelay.cpp  VASTVerse.cpp  VASTThread.cpp VONPeer.cpp  VSOPeer.cpp) 

target_link_libraries (vast ${ACE_LIBRARY})

install(TARGETS vast LIBRARY DESTINATION lib)


