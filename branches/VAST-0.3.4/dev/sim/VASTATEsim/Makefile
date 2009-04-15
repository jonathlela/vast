lib_vastatesim = ../../lib/libvastatesim.a
AR = ar
CC = g++
CFLAGS = -fPIC -c -Wall -I../../include -I../../VASTATE/include

sources = arbitrator_reg.cpp \
		  behavior.cpp \
		  simgame_arb.cpp \
		  simgame.cpp \
		  simgame_peer.cpp \
		  statistics.cpp \
		  vastatesim.cpp

objects = $(subst .cpp,.o,$(sources))

.PHONY: all clean $(lib_vastatesim) noace

all noace: $(lib_vastatesim)

$(lib_vastatesim): $(objects)
	$(AR) rcs $@ $^
								   	
.cpp.o:
	$(CC) $(CFLAGS) $^

clean:
	$(if $(objects), rm -f $(objects))
	$(if $(lib_vastatesim), rm -f $(lib_vastatesim))

noace:
	make TARGET=all
