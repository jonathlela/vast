
lib_vastcommon = ../lib/libvastcommon.a
AR = ar
CC = g++

CFLAGS = -static -c -Wall -I../Dependencies/include/ace -I../Dependencies/include 

ACE_ROOT = ../../ACE_wrappers

#replace -fPIC with -static for static linking
CFLAGS = -static -c -Wall -I$(ACE_ROOT) 

sources_o = Compressor.cpp Errout.cpp Vivaldi.cpp \
            SectionedFile.cpp StdIO_SectionedFile.cpp \
            VoronoiSF.cpp VoronoiSFAlgorithm.cpp \
            MovementGenerator.cpp \
            VASTUtil.cpp
            
sources=$(sources_o)

objects = $(subst .cpp,.o,$(sources))

all: $(lib_vastcommon)	

# $@ refers $(lib_vast) and $^ refers to $(objects)
$(lib_vastcommon): $(objects)
	$(AR) rcs $@ $^
	
.cpp.o:
	$(CC) $(CFLAGS) $^

clean:
	$(if $(objects), rm -f $(objects))
	$(if $(lib_vastcommon), rm -f $(lib_vastcommon))

noace:
	make TARGET=noace
