include_directories("${ACE_INCLUDE_DIR}")

add_definitions("-Wall -static")

add_library (vastcommon Compressor.cpp Errout.cpp SectionedFile.cpp StdIO_SectionedFile.cpp VoronoiSF.cpp VoronoiSFAlgorithm.cpp MovementGenerator.cpp VASTUtil.cpp) 
