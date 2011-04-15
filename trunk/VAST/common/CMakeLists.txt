include_directories("${ACE_INCLUDE_DIR}")

add_definitions("-Wall")

add_library (vastcommon Compressor.cpp Errout.cpp SectionedFile.cpp StdIO_SectionedFile.cpp VoronoiSF.cpp VoronoiSFAlgorithm.cpp MovementGenerator.cpp VASTUtil.cpp) 

target_link_libraries (vastcommon ${ACE_LIBRARY}) 

install(TARGETS vastcommon LIBRARY DESTINATION lib)
