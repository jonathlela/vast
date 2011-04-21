include_directories ("${PROJECT_SOURCE_DIR}/common" "${ACE_INCLUDE_DIR}")

add_definitions("-Wall -g")

add_library (vastnet MessageHandler.cpp MessageQueue.cpp net_emu.cpp net_emubridge.cpp net_manager.cpp VASTnet.cpp net_ace.cpp net_ace_handler.cpp) 

target_link_libraries (vastnet ${ACE_LIBRARY})

install(TARGETS vastnet LIBRARY DESTINATION lib)
