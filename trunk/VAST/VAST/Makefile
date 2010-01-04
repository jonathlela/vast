
#VASTcommon := ../common

lib_vast = ../lib/libvast.a
AR = ar
CC = g++

ACE_ROOT = ../../ACE_wrappers/

CFLAGS = -static -c -Wall -I../common -I../VASTnet  -I../../ACE_wrappers

sources = IDGenerator.cpp Relay.cpp Topology.cpp VASTVerse.cpp VONPeer.cpp
 
objects = $(subst .cpp,.o,$(sources))

#VAST depends on VASTcommon ?
#$(lib_vast): $(VASTcommon) 
 
all: $(lib_vast) 	

# $@ refers $(lib_vast) and $^ refers to $(objects)
$(lib_vast): $(objects)
	$(AR) rcs $@ $^
	
.cpp.o:
	$(CC) $(CFLAGS) $^

clean:
	$(if $(objects), rm -f $(objects))
	$(if $(lib_vast), rm -f $(lib_vast))

noace:
