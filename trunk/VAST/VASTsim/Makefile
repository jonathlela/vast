
lib_vastsim = ../lib/libvastsim.a
AR = ar
CC = g++
CFLAGS = -static -c -Wall -I../common 

sources = SimNode.cpp  VASTsim.cpp
objects = $(subst .cpp,.o,$(sources))

all: $(lib_vastsim)	

# $@ refers $(lib_vast) and $^ refers to $(objects)
$(lib_vastsim): $(objects)
	$(AR) rcs $@ $^
	
.cpp.o:
	$(CC) $(CFLAGS) $^

clean:
	$(if $(objects), rm -f $(objects))
	$(if $(lib_vastsim), rm -f $(lib_vastsim))

