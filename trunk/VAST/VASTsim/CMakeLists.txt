include_directories ("${PROJECT_SOURCE_DIR}/common" "${ACE_INCLUDE_DIR}")

add_definitions("-Wall")

add_library (vastsim SimNode.cpp  VASTsim.cpp) 

target_link_libraries (vastsim ${ACE_LIBRARY})

install(TARGETS vastsim LIBRARY DESTINATION lib)


