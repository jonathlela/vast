
lib_vastatesim = ../lib/libvastatesim.a
AR = ar
CC = g++
CFLAGS = -static -c -Wall -I../common -I../VAST -I../VASTsim

sources = SimAgent.cpp  SimArbitrator.cpp  SimPeer.cpp  VASTATEsim.cpp

objects = $(subst .cpp,.o,$(sources))

all: $(lib_vastatesim)	

# $@ refers $(lib_vast) and $^ refers to $(objects)
$(lib_vastatesim): $(objects)
	$(AR) rcs $@ $^
	
.cpp.o:
	$(CC) $(CFLAGS) $^

clean:
	$(if $(objects), rm -f $(objects))
	$(if $(lib_vastatesim), rm -f $(lib_vastatesim))

