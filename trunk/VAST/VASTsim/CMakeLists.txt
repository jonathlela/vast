include_directories ("${PROJECT_SOURCE_DIR}/common" "${ACE_INCLUDE_DIR}")

add_definitions("-Wall -static")

add_library (vastsim SimNode.cpp  VASTsim.cpp) 
